#pragma once

#include <span>
#include <limits>

#include "types.hpp"
#include "errors.hpp"
#include "segment.hpp"
#include "commands.hpp"
#include "congestion_controller.hpp"
#include "shared_ctx.hpp"
#include "sender_buffer.hpp"
#include "utility.hpp"
#include "results.hpp"
#include "flusher.hpp"

namespace imkcpp {
    template <size_t MTU>
    class Sender final {
        constexpr static size_t MAX_SEGMENT_SIZE = MTU_TO_MSS<MTU>();

        SharedCtx& shared_ctx;
        CongestionController<MTU>& congestion_controller;
        RtoCalculator& rto_calculator;
        Flusher<MTU>& flusher;
        SenderBuffer& sender_buffer;
        SegmentTracker& segment_tracker;

        std::deque<Segment> snd_queue{};

        u32 fastresend = 0;
        u32 fastlimit = constants::IKCP_FASTACK_LIMIT;
        u32 nodelay = 0;

        u32 xmit = 0;
        u32 dead_link = constants::IKCP_DEADLINK;

    public:
        explicit Sender(SharedCtx& shared_ctx,
                        CongestionController<MTU>& congestion_controller,
                        RtoCalculator& rto_calculator,
                        Flusher<MTU>& flusher,
                        SenderBuffer& sender_buffer,
                        SegmentTracker& segment_tracker) :
                        shared_ctx(shared_ctx),
                        congestion_controller(congestion_controller),
                        rto_calculator(rto_calculator),
                        flusher(flusher),
                        sender_buffer(sender_buffer),
                        segment_tracker(segment_tracker) {}

        /// Takes the payload, splits it into segments and puts them into the send queue.
        [[nodiscard]] tl::expected<size_t, error> send(const std::span<const std::byte> buffer) {
            if (buffer.empty()) {
                return tl::unexpected(error::buffer_too_small);
            }

            const size_t count = estimate_segments_count(buffer.size());

            if (count > std::numeric_limits<u8>::max()) {
                return tl::unexpected(error::too_many_fragments);
            }

            // We should limit sending by receive window because we can send more than send_wnd segments,
            // but cannot receive more than rcv_wnd segments.
            if (count > this->congestion_controller.get_receive_window()) {
                return tl::unexpected(error::exceeds_window_size);
            }

            size_t offset = 0;

            for (size_t i = 0; i < count; i++) {
                const size_t size = std::min(buffer.size() - offset, MAX_SEGMENT_SIZE);
                assert(size > 0);

                Segment& seg = this->snd_queue.emplace_back();
                seg.data_assign({buffer.data() + offset, size});
                seg.header.frg = static_cast<u8>(count - i - 1);

                assert(seg.data_size() == size);

                offset += size;
            }

            return offset;
        }

        /// Flushes data segments from the send queue to the sender buffer.
        void move_send_queue_to_buffer(const u32 cwnd, const u32 current, const i32 unused_receive_window) {
            while (!this->snd_queue.empty() && this->segment_tracker.get_snd_nxt() < this->segment_tracker.get_snd_una() + cwnd) {
                Segment& newseg = this->snd_queue.front();

                newseg.header.conv = this->shared_ctx.get_conv();
                newseg.header.cmd = commands::IKCP_CMD_PUSH;
                newseg.header.wnd = unused_receive_window;
                newseg.header.ts = current;
                newseg.header.sn = this->segment_tracker.get_and_increment_snd_nxt();
                newseg.header.una = this->segment_tracker.get_rcv_nxt();

                newseg.metadata.resendts = current;
                newseg.metadata.rto = this->rto_calculator.get_rto();
                newseg.metadata.fastack = 0;
                newseg.metadata.xmit = 0;

                this->sender_buffer.push_segment(newseg);
                this->snd_queue.pop_front();
            }
        }

        /// Given current max segment size, estimates the number of segments needed to fit the payload.
        // ReSharper disable once CppMemberFunctionMayBeStatic
        [[nodiscard]] size_t estimate_segments_count(const size_t size) const {
            return std::max(static_cast<size_t>(1), (size + MAX_SEGMENT_SIZE - 1) / MAX_SEGMENT_SIZE);
        }

        void set_fastresend(const u32 value) {
            this->fastresend = value;
        }

        void set_fastlimit(const u32 value) {
            this->fastlimit = value;
        }

        void set_nodelay(const u32 value) {
            this->nodelay = value;
            this->rto_calculator.set_min_rto(value > 0 ? constants::IKCP_RTO_NDL : constants::IKCP_RTO_MIN);
        }

        void set_deadlink(const u32 value) {
            this->dead_link = value;
        }

        /// Flushes data segments from the send queue to the output callback.
        void flush_data_segments(FlushResult& flush_result, const output_callback_t& output, const u32 current, const i32 unused_receive_window) {
            const u32 cwnd = this->congestion_controller.calculate_congestion_window();
            this->move_send_queue_to_buffer(cwnd, current, unused_receive_window);

            bool change = false;

            const u32 resent = (this->fastresend > 0) ? this->fastresend : 0xffffffff;
            const u32 rtomin = (this->nodelay == 0) ? (this->rto_calculator.get_rto() >> 3) : 0;

            const auto has_never_been_sent = [](const Segment& segment) -> bool {
                return segment.metadata.xmit == 0;
            };

            const auto prepare_for_first_send = [&](Segment& segment) -> void {
                segment.metadata.xmit++;
                segment.metadata.rto = this->rto_calculator.get_rto();
                segment.metadata.resendts = current + segment.metadata.rto + rtomin;
            };

            const auto has_timed_out = [&](const Segment& segment) -> bool {
                return time_delta(current, segment.metadata.resendts) >= 0;
            };

            const auto prepare_segment_for_resend = [&](Segment& segment) -> void {
                segment.metadata.xmit++;
                ++this->xmit;

                if (this->nodelay == 0) {
                    segment.metadata.rto += std::max(segment.metadata.rto, this->rto_calculator.get_rto());
                } else {
                    const u32 step = this->nodelay < 2? segment.metadata.rto : this->rto_calculator.get_rto();
                    segment.metadata.rto += step / 2;
                }

                segment.metadata.resendts = current + segment.metadata.rto;
            };

            const auto can_fast_resend = [&](const Segment& segment) -> bool {
                return resent < segment.metadata.fastack && (segment.metadata.xmit < this->fastlimit || this->fastlimit == 0);
            };

            const auto prepare_segment_for_fast_resend = [&](Segment& segment) {
                segment.metadata.xmit++;
                segment.metadata.fastack = 0;
                segment.metadata.resendts = current + segment.metadata.rto;
            };

            const auto send_segment = [&](Segment& segment) -> void {
                segment.header.ts = current;
                segment.header.wnd = unused_receive_window;
                segment.header.una = this->segment_tracker.get_rcv_nxt();

                flush_result.total_bytes_sent += this->flusher.flush_if_does_not_fit(output, segment.data_size());
                this->flusher.emplace(segment.header, segment.data);

                if (segment.metadata.xmit >= this->dead_link) {
                    this->shared_ctx.set_state(State::DeadLink);
                }
            };

            auto process_segment = [&](Segment& segment) -> bool {
                if (has_never_been_sent(segment)) {
                    prepare_for_first_send(segment);
                    return true;
                }

                if (has_timed_out(segment)) {
                    prepare_segment_for_resend(segment);
                    flush_result.timeout_retransmitted_count++;
                    return true;
                }

                if (can_fast_resend(segment)) {
                    prepare_segment_for_fast_resend(segment);
                    flush_result.fast_retransmitted_count++;
                    change = true;
                    return true;
                }

                return false;
            };

            const auto begin = this->sender_buffer.begin();
            const auto end = this->sender_buffer.end();

            std::for_each(begin, end, [&](Segment& segment) {
                if (process_segment(segment)) {
                    send_segment(segment);
                    flush_result.cmd_push_count++;
                }
            });

            if (change) {
                this->congestion_controller.packets_resent(this->segment_tracker.get_packets_in_flight_count(), resent);
            }

            if (flush_result.timeout_retransmitted_count > 0) {
                this->congestion_controller.packet_lost();
            }
        }
    };
}