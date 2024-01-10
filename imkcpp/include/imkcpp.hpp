#pragma once

#include <functional>
#include <span>
#include <queue>
#include <optional>
#include <cstddef>
#include "third_party/expected.hpp"

#include "types.hpp"
#include "constants.hpp"
#include "segment.hpp"
#include "state.hpp"
#include "errors.hpp"
#include "results.hpp"
#include "rto_calculator.hpp"
#include "congestion_controller.hpp"
#include "shared_ctx.hpp"
#include "receiver.hpp"
#include "sender_buffer.hpp"
#include "sender.hpp"
#include "ack_controller.hpp"
#include "flusher.hpp"

namespace imkcpp {
    // TODO: Some day later this should become a template with MTU and max segment size as template parameters
    // TODO: because changing MTU at runtime will cause issues with already queued segments.
    // TODO: Additionally, this will allow more compile-time optimizations.
    class ImKcpp final {
        SharedCtx shared_ctx{};
        Flusher flusher{shared_ctx};
        SenderBuffer sender_buffer{shared_ctx};

        RtoCalculator rto_calculator{shared_ctx};
        CongestionController congestion_controller{flusher, shared_ctx};

        AckController ack_controller{flusher, rto_calculator, sender_buffer, shared_ctx};

        Receiver receiver{congestion_controller, shared_ctx};
        Sender sender{congestion_controller, rto_calculator, flusher, sender_buffer, ack_controller, shared_ctx};

        bool updated = false;
        u32 current = 0; // TODO: Should probably move to shared_ctx as well, but idk
        u32 ts_flush = constants::IKCP_INTERVAL;

        [[nodiscard]] auto create_service_segment(i32 unused_receive_window) const -> Segment {
            Segment seg;

            seg.header.conv = this->shared_ctx.conv;
            seg.header.cmd = commands::IKCP_CMD_ACK;
            seg.header.frg = 0;
            seg.header.wnd = unused_receive_window;
            seg.header.una = shared_ctx.rcv_nxt;
            seg.header.sn = 0;
            seg.header.ts = 0;
            seg.header.len = 0;

            return seg;
        }

    public:
        explicit ImKcpp(u32 conv) {
            this->shared_ctx.conv = conv;
            this->set_mtu(constants::IKCP_MTU_DEF);
        }

        auto set_interval(u32 interval) -> void {
            this->shared_ctx.interval = std::clamp(interval, static_cast<u32>(10), static_cast<u32>(5000));
        }

        auto set_nodelay(i32 nodelay) -> void {
            this->sender.set_nodelay(nodelay);
        }

        auto set_resend(i32 resend) -> void {
            this->sender.set_fastresend(resend);
        }

        auto set_mtu(u32 mtu) -> tl::expected<size_t, error> {
            if (mtu <= constants::IKCP_OVERHEAD) {
                return tl::unexpected(error::less_than_header_size);
            }

            // TODO: Does this really need triple the size?
            this->flusher.resize(static_cast<size_t>(mtu + constants::IKCP_OVERHEAD) * 3);
            this->shared_ctx.mtu = mtu;
            this->shared_ctx.mss = this->shared_ctx.mtu - constants::IKCP_OVERHEAD;

            return mtu;
        }

        auto set_wndsize(u32 sndwnd, u32 rcvwnd) -> void {
            if (sndwnd > 0) {
                this->congestion_controller.set_send_window(sndwnd);
                this->congestion_controller.set_remote_window(sndwnd);
            }

            if (rcvwnd > 0) {
                this->congestion_controller.set_receive_window(rcvwnd);
            }
        }

        auto set_congestion_window_enabled(bool state) -> void {
            this->congestion_controller.set_congestion_window_enabled(state);
        }

        auto input(std::span<const std::byte> data) -> tl::expected<size_t, error> {
            if (data.size() < constants::IKCP_OVERHEAD) {
                return tl::unexpected(error::less_than_header_size);
            }

            if (data.size() > this->shared_ctx.mtu) {
                return tl::unexpected(error::more_than_mtu);
            }

            const u32 prev_una = shared_ctx.snd_una;
            FastAckCtx fastack_ctx{};

            size_t offset = 0;

            while (offset <= data.size()) {
                if (data.size() - offset < constants::IKCP_OVERHEAD) {
                    break;
                }

                SegmentHeader header;
                header.decode_from(data, offset);

                if (header.conv != this->shared_ctx.conv) {
                    return tl::unexpected(error::conv_mismatch);
                }

                if (header.len > data.size() - offset) {
                    return tl::unexpected(error::header_and_payload_length_mismatch);
                }

                if (!commands::is_valid(header.cmd)) {
                    return tl::unexpected(error::unknown_command);
                }

                this->congestion_controller.set_remote_window(header.wnd);
                this->ack_controller.una_received(header.una);

                switch (header.cmd) {
                    case commands::IKCP_CMD_ACK: {
                        this->ack_controller.ack_received(this->current, header.sn, header.ts);
                        fastack_ctx.update(header.sn, header.ts);
                        break;
                    }
                    case commands::IKCP_CMD_PUSH: {
                        if (!this->congestion_controller.fits_receive_window(header.sn)) {
                            break;
                        }

                        this->ack_controller.schedule_ack(header.sn, header.ts);

                        // TODO: This could probably be in fits_receive_window check,
                        // TODO: but I'm not sure if it's correct because
                        // TODO: the original code does it after schedling ack
                        if (header.sn >= shared_ctx.rcv_nxt) {
                            Segment seg;
                            seg.header = header; // TODO: Remove this copy
                            seg.data.decode_from(data, offset, header.len);

                            this->receiver.parse_data(seg);
                        }
                        break;
                    }
                    case commands::IKCP_CMD_WASK: {
                        this->congestion_controller.set_probe_flag(constants::IKCP_ASK_TELL);
                        break;
                    }
                    case commands::IKCP_CMD_WINS: {
                        break;
                    }
                    default: {
                        return tl::unexpected(error::unknown_command);
                    }
                }
            }

            this->ack_controller.acknowledge_fastack(fastack_ctx);
            congestion_controller.adjust_parameters(shared_ctx.snd_una, prev_una);

            return offset;
        }

        auto recv(std::span<std::byte> buffer) -> tl::expected<size_t, error> {
            return this->receiver.recv(buffer);
        }

        auto send(std::span<const std::byte> buffer) -> tl::expected<size_t, error> {
            return this->sender.send(buffer);
        }

        auto check(u32 current) -> u32 {
            if (!this->updated) {
                return current;
            }

            if (std::abs(time_delta(current, this->ts_flush)) >= 10000) {
                this->ts_flush = current;
            }

            if (time_delta(current, this->ts_flush) >= 0) {
                return current;
            }

            const u32 next_flush = static_cast<u32>(std::max(0, time_delta(this->ts_flush, current)));
            const std::optional<u32> earliest_transmit = this->sender.get_earliest_transmit_delta(current);

            u32 minimal = 0;

            if (earliest_transmit.has_value()) {
                minimal = earliest_transmit.value() < next_flush ? earliest_transmit.value() : next_flush;
            } else {
                minimal = next_flush;
            }

            return current + std::min(this->shared_ctx.interval, minimal);
        }

        auto update(u32 current, const output_callback_t& callback) -> FlushResult {
            this->current = current;

            if (!this->updated) {
                this->updated = true;
                this->ts_flush = this->current;
            }

            i32 slap = time_delta(this->current, this->ts_flush);

            if (slap >= 10000 || slap < -10000) {
                this->ts_flush = this->current;
                slap = 0;
            }

            if (slap >= 0) {
                this->ts_flush += this->shared_ctx.interval;
                if (time_delta(this->current, this->ts_flush) >= 0) {
                    this->ts_flush = this->current + this->shared_ctx.interval;
                }
                return this->flush(callback);
            }

            return {};
        }

        auto flush(const output_callback_t& callback) -> FlushResult {
            // TODO: Populate result with information
            FlushResult result;

            if (!this->updated) {
                return result;
            }

            const u32 current = this->current;
            const i32 unused_receive_window = this->receiver.get_unused_receive_window();

            // Service data
            Segment seg = this->create_service_segment(unused_receive_window);
            this->ack_controller.flush_acks(callback, seg);
            this->congestion_controller.update_probe_request(current);
            this->congestion_controller.flush_probes(callback, seg);

            // Useful data
            this->sender.flush_data_segments(callback, current, unused_receive_window);

            flusher.flush_if_not_empty(callback);

            return result;
        }

        [[nodiscard]] auto get_state() const -> State {
            return this->shared_ctx.get_state();
        }

        [[nodiscard]] auto get_mtu() const -> u32 {
            return this->shared_ctx.mtu;
        }
        [[nodiscard]] auto get_max_segment_size() const -> u32 {
            return this->shared_ctx.mss;
        }

        [[nodiscard]] auto peek_size() const -> tl::expected<size_t, error> {
            return this->receiver.peek_size();
        }

        [[nodiscard]] auto estimate_segments_count(size_t size) const -> size_t {
            return this->sender.estimate_segments_count(size);
        }
    };
}