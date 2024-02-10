#pragma once

#include <span>
#include <queue>
#include <optional>
#include <cstddef>
#include <limits>
#include "third_party/expected.hpp"

#include "types.hpp"
#include "clock.hpp"
#include "constants.hpp"
#include "segment.hpp"
#include "state.hpp"
#include "errors.hpp"
#include "results.hpp"
#include "rto_calculator.hpp"
#include "congestion_controller.hpp"
#include "window_prober.hpp"
#include "shared_ctx.hpp"
#include "receiver.hpp"
#include "sender_buffer.hpp"
#include "sender.hpp"
#include "ack_controller.hpp"
#include "flusher.hpp"
#include "utility.hpp"
#include "commands.hpp"

namespace imkcpp {
    /// The main class of the library.
    template <size_t MTU>
    class ImKcpp final {
        static_assert(MTU > SegmentHeader::OVERHEAD, "MTU is too small");

        constexpr static size_t MAX_SEGMENT_SIZE = MTU_TO_MSS<MTU>();

        SharedCtx shared_ctx{};
        Flusher<MTU> flusher{};
        SegmentTracker segment_tracker{};
        RtoCalculator rto_calculator{};
        CongestionController<MTU> congestion_controller{};
        WindowProber window_prober{};
        Receiver receiver{};

        SenderBuffer sender_buffer{};
        AckController ack_controller{sender_buffer, segment_tracker};
        Sender<MTU> sender{shared_ctx, congestion_controller, rto_calculator, flusher, sender_buffer, segment_tracker};

        bool updated = false; // Whether update() was called at least once
        timepoint_t current; // Current / last time we updated the state
        timepoint_t ts_flush = timepoint_t(milliseconds_t(constants::IKCP_INTERVAL)); // Time when we will probably flush the data next time

        // Creates a new service header for non-data packets.
        [[nodiscard]] auto create_service_header(const i32 unused_receive_window) const noexcept -> SegmentHeader {
            SegmentHeader header;

            header.conv = this->shared_ctx.get_conv();
            header.cmd = commands::ACK;
            header.frg = Fragment{0};
            header.wnd = unused_receive_window;
            header.una = this->receiver.get_rcv_nxt();
            header.sn = 0;
            header.ts = timepoint_t{};
            header.len = PayloadLen(0);

            return header;
        }

    public:
        explicit ImKcpp(const Conv conv) noexcept {
            this->shared_ctx.set_conv(conv);
            this->set_receive_window(constants::IKCP_WND_RCV);
            this->set_send_window(constants::IKCP_WND_SND);
        }

        /// Sets the internal clock interval in milliseconds. Must be between 10 and 5000.
        auto set_interval(duration_t interval) noexcept -> void {
            interval = std::clamp<duration_t>(interval, 10ms, 5000ms);

            this->shared_ctx.set_interval(interval);
            this->rto_calculator.set_interval(interval);
        }

        auto set_nodelay(const u32 nodelay) noexcept -> void {
            this->sender.set_nodelay(nodelay);
        }

        /// Sets the number of non-sequential acks required to trigger fast resend.
        auto set_fastresend(const u32 fastresend) noexcept -> void {
            this->sender.set_fastresend(fastresend);
        }

        /// Sets the maximum count of fast resends per segment.
        auto set_fastlimit(const u32 limit) noexcept -> void {
            this->sender.set_fastlimit(limit);
        }

        /// Sets the maximum number of segments that can be sent in a single update() call.
        auto set_send_window(const u32 sndwnd) noexcept -> void {
            assert(sndwnd > 0);

            this->congestion_controller.set_send_window(sndwnd);
            this->congestion_controller.set_remote_window(sndwnd);
        }

        /// Sets the maximum number of segments that can be queued in the receive buffer.
        auto set_receive_window(const u32 rcvwnd) noexcept -> void {
            assert(rcvwnd > 0);

            this->congestion_controller.set_receive_window(rcvwnd);
            this->ack_controller.reserve(rcvwnd);
            this->receiver.set_queue_limit(rcvwnd);
        }

        /// Enables or disables congestion window.
        auto set_congestion_window_enabled(const bool state) noexcept -> void {
            this->congestion_controller.set_congestion_window_enabled(state);
        }

        /// Sets maximum retransmission count for a single segment until it's considered lost.
        auto set_deadlink(const u32 threshold) noexcept -> void {
            this->sender.set_deadlink(threshold);
        }

        /// Receives data from the transport layer.
        auto input(const std::span<const std::byte> data) noexcept -> tl::expected<InputResult, error> {
            if (data.size() < SegmentHeader::OVERHEAD) {
                return tl::unexpected(error::less_than_header_size);
            }

            InputResult input_result{};

            const u32 prev_una = this->segment_tracker.get_snd_una();
            FastAckCtx fastack_ctx{};

            SegmentHeader header;
            size_t offset = 0;

            const auto drop_push = [&] {
                offset += header.len.get();
                input_result.dropped_push_count++;
            };

            const auto data_size = data.size();

            while (true) {
                if (data_size - offset < SegmentHeader::OVERHEAD) {
                    break;
                }

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

                switch (header.cmd.get()) {
                    case commands::PUSH.get(): {
                        if (!this->congestion_controller.fits_receive_window(this->receiver.get_rcv_nxt(), header.sn)) {
                            drop_push();
                            break;
                        }

                        this->ack_controller.schedule_ack(header.sn, header.ts);

                        if (this->receiver.should_receive(header.sn)) {
                            SegmentData segment_data;
                            segment_data.decode_from(data, offset, header.len.get());

                            this->receiver.emplace_segment(header, segment_data);
                        } else {
                            drop_push();
                        }
                        break;
                    }
                    case commands::ACK.get(): {
                        this->rto_calculator.update_rto(this->current, header.ts);
                        this->ack_controller.ack_received(header.sn);
                        fastack_ctx.update(header.sn, header.ts);
                        input_result.cmd_ack_count++;
                        break;
                    }
                    case commands::WASK.get(): {
                        this->window_prober.set_flag(ProbeFlag::AskTell);
                        input_result.cmd_wask_count++;
                        break;
                    }
                    case commands::WINS.get(): {
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

        /// Reads data from the receive queue.
        auto recv(const std::span<std::byte> buffer) noexcept -> tl::expected<size_t, error> {
            const auto rcv_wnd = this->congestion_controller.get_receive_window();
            const auto result = this->receiver.recv(buffer, rcv_wnd);

            if (result.has_value()) {
                const auto& result_value = result.value();

                if (result_value.recovered) {
                    this->window_prober.set_flag(ProbeFlag::AskTell);
                }

                return result_value.size;
            }

            return tl::unexpected(result.error());
        }

        /// Sends data.
        auto send(const std::span<const std::byte> buffer) noexcept -> tl::expected<size_t, error> {
            return this->sender.send(buffer);
        }

        /// Checks when the next update() should be called.
        auto check(const timepoint_t current) noexcept -> timepoint_t {
            if (!this->updated) {
                return current;
            }

            if (time_delta(current, this->ts_flush) >= 0) {
                return current;
            }

            const duration_t next_flush = std::max(duration_t{0}, this->ts_flush - current);
            const std::optional<duration_t> earliest_transmit = this->sender_buffer.get_earliest_transmit_delta(current);

            duration_t minimal;

            if (earliest_transmit.has_value()) {
                minimal = earliest_transmit.value() < next_flush ? earliest_transmit.value() : next_flush;
            } else {
                minimal = next_flush;
            }

            return current + std::min(this->shared_ctx.get_interval(), minimal);
        }

        /// Checks when the next update() should be called. Uses the current time.
        auto check() noexcept -> timepoint_t {
            return this->check(std::chrono::steady_clock::now());
        }

        /// Updates the state and performs flush if necessary.
        auto update(const timepoint_t current, const output_callback_t& callback) noexcept -> FlushResult {
            this->current = current;

            if (!this->updated) {
                this->updated = true;
                this->ts_flush = this->current;
            }

            if (this->current >= this->ts_flush) {
                const duration_t interval = this->shared_ctx.get_interval();
                this->ts_flush += interval;

                if (this->current >= this->ts_flush) {
                    this->ts_flush = this->current + interval;
                }

                return this->flush(callback);
            }

            return {};
        }

        /// Updates the state and performs flush if necessary. Uses the current time.
        auto update(const output_callback_t& callback) noexcept -> FlushResult {
            return this->update(std::chrono::steady_clock::now(), callback);
        }

        /// Flushes the data to the output callback.
        auto flush(const output_callback_t& callback) noexcept -> FlushResult {
            FlushResult flush_result{};

            if (!this->updated) {
                return flush_result;
            }

            const timepoint_t current = this->current;
            const i32 unused_receive_window = std::max(static_cast<i32>(this->congestion_controller.get_receive_window()) - static_cast<i32>(this->receiver.size()), 0);

            SegmentHeader header = this->create_service_header(unused_receive_window);

            const auto flush_acks = [&] {
                for (const Ack& ack : this->ack_controller) {
                    flush_result.total_bytes_sent += this->flusher.flush_if_full(callback);

                    header.sn = ack.sn;
                    header.ts = ack.ts;

                    this->flusher.emplace(header);
                }

                flush_result.cmd_ack_count += this->ack_controller.size();
                this->ack_controller.clear();
            };

            const auto flush_probes = [&] {
                this->window_prober.update(current, this->congestion_controller.get_remote_window());

                if (this->window_prober.has_flag(ProbeFlag::AskSend)) {
                    flush_result.total_bytes_sent += this->flusher.flush_if_full(callback);

                    header.cmd = commands::WASK;
                    this->flusher.emplace(header);

                    flush_result.cmd_wask_count++;
                }

                if (this->window_prober.has_flag(ProbeFlag::AskTell)) {
                    flush_result.total_bytes_sent +=this->flusher.flush_if_full(callback);

                    header.cmd = commands::WINS;
                    this->flusher.emplace(header);

                    flush_result.cmd_wins_count++;
                }

                this->window_prober.reset_flags();
            };

            // Acks
            flush_acks();

            // Window probes
            flush_probes();

            // Useful data
            const u32 rcv_nxt = this->receiver.get_rcv_nxt();
            this->sender.flush_data_segments(flush_result, callback, current, unused_receive_window, rcv_nxt);

            // Flush remaining
            flush_result.total_bytes_sent += this->flusher.flush_if_not_empty(callback);

            this->congestion_controller.ensure_at_least_one_packet_in_flight();

            return flush_result;
        }

        /// Gets the current state.
        [[nodiscard]] auto get_state() const noexcept -> State {
            return this->shared_ctx.get_state();
        }

        /// Gets bytes count of the available data in the receive queue.
        [[nodiscard]] auto peek_size() const noexcept -> tl::expected<size_t, error> {
            return this->receiver.peek_size();
        }

        /// Calculates the number of segments required to send the given amount of data.
        [[nodiscard]] auto estimate_segments_count(const size_t size) const noexcept -> size_t {
            return this->sender.estimate_segments_count(size);
        }

        /// Calculates the maximum payload size that can be sent in a single send() call.
        [[nodiscard]] auto estimate_max_payload_size() const noexcept -> size_t {
            // We use receive window here because it's the most restrictive value.
            return MAX_SEGMENT_SIZE * std::min(
                static_cast<size_t>(this->congestion_controller.get_receive_window()),
                static_cast<size_t>(std::numeric_limits<Fragment::UT>::max()));
        }
    };
}