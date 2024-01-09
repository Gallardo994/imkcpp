#pragma once

#include <span>
#include "types.hpp"
#include "errors.hpp"
#include "segment.hpp"
#include "commands.hpp"
#include "congestion_controller.hpp"
#include "shared_ctx.hpp"

namespace imkcpp {
    // TODO: Move snd_buf to AckController as it is generally used there for ACKs information
    class Sender {
        CongestionController& congestion_controller;
        RtoCalculator& rto_calculator;
        Flusher& flusher;
        SharedCtx& shared_ctx;

        std::deque<Segment> snd_queue{};
        std::deque<Segment> snd_buf{};

        i32 fastresend = 0;
        i32 fastlimit = constants::IKCP_FASTACK_LIMIT;
        u32 nodelay = 0;

        u32 xmit = 0;
        u32 dead_link = constants::IKCP_DEADLINK;

    public:
        explicit Sender(CongestionController& congestion_controller, RtoCalculator& rto_calculator, Flusher& flusher, SharedCtx& shared_ctx) :
            congestion_controller(congestion_controller), rto_calculator(rto_calculator), flusher(flusher), shared_ctx(shared_ctx) {}

        [[nodiscard]] tl::expected<size_t, error> send(const std::span<const std::byte> buffer) {
            if (buffer.empty()) {
                return tl::unexpected(error::buffer_too_small);
            }

            const size_t count = estimate_segments_count(buffer.size());

            if (count > std::numeric_limits<u8>::max()) {
                return tl::unexpected(error::too_many_fragments);
            }

            if (count > this->congestion_controller.get_send_window()) {
                return tl::unexpected(error::exceeds_window_size);
            }

            size_t offset = 0;

            for (size_t i = 0; i < count; i++) {
                const size_t size = std::min(buffer.size() - offset, this->shared_ctx.mss);
                assert(size > 0);

                Segment& seg = snd_queue.emplace_back();
                seg.data_assign({buffer.data() + offset, size});
                seg.header.frg = static_cast<u8>(count - i - 1);

                assert(seg.data_size() == size);

                offset += size;
            }

            return offset;
        }

        void move_send_queue_to_buffer(const u32 cwnd, const u32 current, const i32 unused_receive_window) {
            while (!snd_queue.empty() && shared_ctx.snd_nxt < shared_ctx.snd_una + cwnd) {
                Segment& newseg = snd_queue.front();

                newseg.header.conv = this->shared_ctx.conv;
                newseg.header.cmd = commands::IKCP_CMD_PUSH;
                newseg.header.wnd = unused_receive_window;
                newseg.header.ts = current;
                newseg.header.sn = shared_ctx.snd_nxt++;
                newseg.header.una = shared_ctx.rcv_nxt;

                newseg.metadata.resendts = current;
                newseg.metadata.rto = this->rto_calculator.get_rto();
                newseg.metadata.fastack = 0;
                newseg.metadata.xmit = 0;

                snd_buf.push_back(std::move(newseg));
                snd_queue.pop_front();
            }
        }

        [[nodiscard]] size_t estimate_segments_count(const size_t size) const {
            return std::max(static_cast<size_t>(1), (size + this->shared_ctx.mss - 1) / this->shared_ctx.mss);
        }

        void acknowledge_seqence_number(const u32 sn) {
            for (auto it = this->snd_buf.begin(); it != this->snd_buf.end();) {
                if (sn == it->header.sn) {
                    this->snd_buf.erase(it);
                    break;
                }

                if (time_delta(sn, it->header.sn) < 0) {
                    break;
                }

                ++it;
            }
        }

        void acknowledge_fastack(const u32 sn, const u32 ts) {
            for (Segment& seg : this->snd_buf) {
                if (time_delta(sn, seg.header.sn) < 0) {
                    break;
                }

                if (sn != seg.header.sn) {
#ifndef IKCP_FASTACK_CONSERVE
                    seg.metadata.fastack++;
#else
                    if (time_delta(ts, seg.ts) >= 0) {
                        seg.metadata.fastack++;
                    }
#endif
                }
            }
        }

        void clear_acknowledged(const u32 una) {
            for (auto it = this->snd_buf.begin(); it != this->snd_buf.end();) {
                if (time_delta(una, it->header.sn) > 0) {
                    it = this->snd_buf.erase(it);
                } else {
                    break;
                }
            }
        }

        void shrink_buf() {
            if (!this->snd_buf.empty()) {
                const Segment& seg = this->snd_buf.front();
                shared_ctx.snd_una = seg.header.sn;
            } else {
                shared_ctx.snd_una = shared_ctx.snd_nxt;
            }
        }

        void set_fastresend(const i32 value) {
            if (value >= 0) {
                this->fastresend = value;
            }
        }

        void set_nodelay(const u32 value) {
            if (value >= 0) {
                this->nodelay = value;
                this->rto_calculator.set_min_rto(value > 0 ? constants::IKCP_RTO_NDL : constants::IKCP_RTO_MIN);
            }
        }

        void flush_data_segments(const output_callback_t& output, const u32 current, const i32 unused_receive_window) {
            // TODO: Return information back to the caller
            const u32 cwnd = this->congestion_controller.calculate_congestion_window();
            this->move_send_queue_to_buffer(cwnd, current, unused_receive_window);

            bool change = false;
            bool lost = false;

            const u32 resent = (this->fastresend > 0) ? this->fastresend : 0xffffffff;
            const u32 rtomin = (this->nodelay == 0) ? (this->rto_calculator.get_rto() >> 3) : 0;

            for (Segment& segment : this->snd_buf) {
                bool needsend = false;

                if (segment.metadata.xmit == 0) {
                    needsend = true;
                    segment.metadata.xmit++;
                    segment.metadata.rto = this->rto_calculator.get_rto();
                    segment.metadata.resendts = current + segment.metadata.rto + rtomin;
                } else if (time_delta(current, segment.metadata.resendts) >= 0) {
                    needsend = true;
                    segment.metadata.xmit++;
                    this->xmit++;

                    if (this->nodelay == 0) {
                        segment.metadata.rto += std::max(segment.metadata.rto, this->rto_calculator.get_rto());
                    } else {
                        const u32 step = this->nodelay < 2? segment.metadata.rto : this->rto_calculator.get_rto();
                        segment.metadata.rto += step / 2;
                    }

                    segment.metadata.resendts = current + segment.metadata.rto;
                    lost = true;
                } else if (segment.metadata.fastack >= resent) {
                    // TODO: The second check is probably redundant
                    if (segment.metadata.xmit <= this->fastlimit || this->fastlimit <= 0) {
                        // TODO: Why isn't this->xmit incremented here?
                        needsend = true;
                        change = true;

                        segment.metadata.xmit++;
                        segment.metadata.fastack = 0;
                        segment.metadata.resendts = current + segment.metadata.rto;
                    }
                }

                if (needsend) {
                    segment.header.ts = current;
                    segment.header.wnd = unused_receive_window;
                    segment.header.una = shared_ctx.rcv_nxt;

                    flusher.flush_if_does_not_fit(output, segment.data_size());
                    flusher.encode(segment);

                    if (segment.metadata.xmit >= this->dead_link) {
                        this->shared_ctx.set_state(State::DeadLink);
                    }

                    // result.data_sent_count++;
                    // result.retransmitted_count += segment.metadata.xmit > 1 ? 1 : 0;
                }
            }

            if (change) {
                this->congestion_controller.resent(shared_ctx.snd_nxt - shared_ctx.snd_una, resent);
            }

            if (lost) {
                this->congestion_controller.packet_lost();
            }

            this->congestion_controller.ensure_at_least_one_packet_in_flight();
        }

        [[nodiscard]] std::optional<u32> get_earliest_transmit_delta(const u32 current) const {
            if (this->snd_buf.empty()) {
                return std::nullopt;
            }

            constexpr u32 default_value = std::numeric_limits<u32>::max();
            u32 tm_packet = default_value;

            for (const Segment& seg : this->snd_buf) {
                if (seg.metadata.resendts <= current) {
                    return 0;
                }

                tm_packet = std::min<u32>(tm_packet, seg.metadata.resendts - current);
            }

            if (tm_packet == default_value) {
                return std::nullopt;
            }

            return tm_packet;
        }
    };
}