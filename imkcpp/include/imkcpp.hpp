#pragma once

#include <span>
#include <queue>
#include <optional>
#include <cstddef>
#include <limits>
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
#include "utility.hpp"

namespace imkcpp {
    template <size_t MTU>
    class ImKcpp final {
        static_assert(MTU > constants::IKCP_OVERHEAD, "MTU is too small");

        constexpr static size_t MAX_SEGMENT_SIZE = MTU_TO_MSS<MTU>();

        SharedCtx shared_ctx{};

        Flusher<MTU> flusher{shared_ctx};
        SegmentTracker segment_tracker{};
        CongestionController<MTU> congestion_controller{shared_ctx, flusher, segment_tracker};

        ReceiverBuffer<MTU> receiver_buffer{congestion_controller, segment_tracker};
        Receiver<MTU> receiver{receiver_buffer, congestion_controller};

        SenderBuffer sender_buffer{shared_ctx, segment_tracker};
        RtoCalculator rto_calculator{shared_ctx};
        AckController<MTU> ack_controller{flusher, rto_calculator, sender_buffer, segment_tracker};
        Sender<MTU> sender{shared_ctx, congestion_controller, rto_calculator, flusher, sender_buffer, ack_controller, segment_tracker};

        bool updated = false; // Whether update() was called at least once
        u32 current = 0; // Current / last time we updated the state
        u32 ts_flush = constants::IKCP_INTERVAL; // Time when we will probably flush the data next time

        // Creates a new service segment for non-data packets.
        [[nodiscard]] auto create_service_segment(const i32 unused_receive_window) const -> Segment {
            Segment seg;

            seg.header.conv = this->shared_ctx.get_conv();
            seg.header.cmd = commands::IKCP_CMD_ACK;
            seg.header.frg = 0;
            seg.header.wnd = unused_receive_window;
            seg.header.una = this->segment_tracker.get_rcv_nxt();
            seg.header.sn = 0;
            seg.header.ts = 0;
            seg.header.len = 0;

            return seg;
        }

    public:
        explicit ImKcpp(const u32 conv) {
            this->shared_ctx.set_conv(conv);
        }

        // Sets the internal clock interval in milliseconds. Must be between 10 and 5000.
        auto set_interval(const u32 interval) -> void {
            this->shared_ctx.set_interval(std::clamp(interval, static_cast<u32>(10), static_cast<u32>(5000)));
        }

        auto set_nodelay(const u32 nodelay) -> void {
            this->sender.set_nodelay(nodelay);
        }

        auto set_fastresend(const u32 fastresend) -> void {
            this->sender.set_fastresend(fastresend);
        }

        auto set_fastlimit(const u32 limit) -> void {
            this->sender.set_fastlimit(limit);
        }

        // Sets the maximum number of segments that can be sent in a single update() call.
        auto set_send_window(const u32 sndwnd) -> void {
            assert(sndwnd > 0);

            this->congestion_controller.set_send_window(sndwnd);
            this->congestion_controller.set_remote_window(sndwnd);
        }

        // Sets the maximum number of segments that can be queued in the receive buffer.
        auto set_receive_window(const u32 rcvwnd) -> void {
            assert(rcvwnd > 0);

            this->congestion_controller.set_receive_window(rcvwnd);
            this->ack_controller.reserve(rcvwnd);
        }

        // Enables or disables congestion window.
        auto set_congestion_window_enabled(const bool state) -> void {
            this->congestion_controller.set_congestion_window_enabled(state);
        }

        auto set_deadlink(const u32 threshold) -> void {
            this->sender.set_deadlink(threshold);
        }

        [[nodiscard]] u32 get_total_retransmits_count() const {
            return this->sender.get_total_retransmits_count();
        }

        // Receives data from the transport layer.
        // TODO: Return more detailed data on what was received like update()/flush() does.
        auto input(const std::span<const std::byte> data) -> tl::expected<InputResult, error> {
            if (data.size() < constants::IKCP_OVERHEAD) {
                return tl::unexpected(error::less_than_header_size);
            }

            if (data.size() > MTU) {
                return tl::unexpected(error::more_than_mtu);
            }

            InputResult input_result{};

            const u32 prev_una = this->segment_tracker.get_snd_una();
            FastAckCtx fastack_ctx{};

            size_t offset = 0;

            while (offset <= data.size()) {
                if (data.size() - offset < constants::IKCP_OVERHEAD) {
                    break;
                }

                SegmentHeader header;
                header.decode_from(data, offset);

                if (header.conv != this->shared_ctx.get_conv()) {
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
                        input_result.cmd_ack_count++;
                        break;
                    }
                    case commands::IKCP_CMD_PUSH: {
                        const auto drop_push = [&] {
                            offset += header.len;
                            input_result.dropped_push_count++;
                        };

                        if (!this->congestion_controller.fits_receive_window(header.sn)) {
                            drop_push();
                            break;
                        }

                        this->ack_controller.schedule_ack(header.sn, header.ts);

                        if (this->segment_tracker.should_receive(header.sn)) {
                            SegmentData segment_data;
                            segment_data.decode_from(data, offset, header.len);

                            this->receiver_buffer.emplace_segment(header, segment_data);
                            this->receiver.move_receive_buffer_to_queue();
                        } else {
                            drop_push();
                        }
                        break;
                    }
                    case commands::IKCP_CMD_WASK: {
                        this->congestion_controller.set_probe_flag(constants::IKCP_ASK_TELL);
                        input_result.cmd_wask_count++;
                        break;
                    }
                    case commands::IKCP_CMD_WINS: {
                        input_result.cmd_wins_count++;
                        break;
                    }
                    default: {
                        return tl::unexpected(error::unknown_command);
                    }
                }
            }

            this->ack_controller.acknowledge_fastack(fastack_ctx);

            if (this->segment_tracker.get_snd_una() > prev_una) {
                this->congestion_controller.adjust_parameters();
            }

            input_result.total_bytes_received = offset;

            return input_result;
        }

        // Reads data from the receive queue.
        auto recv(const std::span<std::byte> buffer) -> tl::expected<size_t, error> {
            assert(!buffer.empty());

            return this->receiver.recv(buffer);
        }

        // Sends data.
        auto send(const std::span<const std::byte> buffer) -> tl::expected<size_t, error> {
            return this->sender.send(buffer);
        }

        // Checks when the next update() should be called.
        auto check(const u32 current) -> u32 {
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
            const std::optional<u32> earliest_transmit = this->sender_buffer.get_earliest_transmit_delta(current);

            u32 minimal = 0;

            if (earliest_transmit.has_value()) {
                minimal = earliest_transmit.value() < next_flush ? earliest_transmit.value() : next_flush;
            } else {
                minimal = next_flush;
            }

            return current + std::min(this->shared_ctx.get_interval(), minimal);
        }

        // Updates the state and performs flush if necessary.
        auto update(const u32 current, const output_callback_t& callback) -> FlushResult {
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
                const auto interval = this->shared_ctx.get_interval();

                this->ts_flush += interval;
                if (time_delta(this->current, this->ts_flush) >= 0) {
                    this->ts_flush = this->current + interval;
                }

                return this->flush(callback);
            }

            return {};
        }

        // Flushes the data to the output callback.
        auto flush(const output_callback_t& callback) -> FlushResult {
            FlushResult flush_result{};

            if (!this->updated) {
                return flush_result;
            }

            const u32 current = this->current;
            const i32 unused_receive_window = this->receiver.get_unused_receive_window();

            Segment seg = this->create_service_segment(unused_receive_window);

            // Acks
            this->ack_controller.flush_acks(flush_result, callback, seg);

            // Window probes
            this->congestion_controller.update_probe_request(current);
            this->congestion_controller.flush_probes(flush_result, callback, seg);

            // Useful data
            this->sender.flush_data_segments(flush_result, callback, current, unused_receive_window);

            // Flush remaining
            flush_result.total_bytes_sent += this->flusher.flush_if_not_empty(callback);

            this->congestion_controller.ensure_at_least_one_packet_in_flight();

            return flush_result;
        }

        // Gets the current state.
        [[nodiscard]] auto get_state() const -> State {
            return this->shared_ctx.get_state();
        }

        // Gets bytes count of the available data in the receive queue.
        [[nodiscard]] auto peek_size() const -> tl::expected<size_t, error> {
            return this->receiver.peek_size();
        }

        // Calculates the number of segments required to send the given amount of data.
        [[nodiscard]] auto estimate_segments_count(const size_t size) const -> size_t {
            return this->sender.estimate_segments_count(size);
        }

        // Calculates the maximum payload size that can be sent in a single send() call.
        [[nodiscard]] auto estimate_max_payload_size() const -> size_t {
            const size_t max_segments_count = std::min(
                static_cast<size_t>(this->congestion_controller.get_send_window()),
                static_cast<size_t>(std::numeric_limits<u8>::max())
                );

            return MAX_SEGMENT_SIZE * max_segments_count;
        }
    };
}