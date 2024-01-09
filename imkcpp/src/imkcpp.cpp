#include "imkcpp.hpp"
#include "constants.hpp"
#include "commands.hpp"
#include "utility.hpp"

// TODO: > 200 lines of code, gg. Split into different implementation files.
namespace imkcpp {
    ImKcpp::ImKcpp(const u32 conv) {
        this->shared_ctx.conv = conv;
        this->set_mtu(constants::IKCP_MTU_DEF);
    }

    State ImKcpp::get_state() const {
        return this->shared_ctx.get_state();
    }

    void ImKcpp::set_interval(const u32 interval) {
        this->interval = std::clamp(interval, static_cast<u32>(10), static_cast<u32>(5000));
    }

    // TODO: Should return a tl::expected with the new value or an error
    void ImKcpp::set_nodelay(const i32 nodelay) {
        this->sender.set_nodelay(nodelay);
    }

    // TODO: Should return a tl::expected with the new value or an error
    void ImKcpp::set_resend(const i32 resend) {
        this->sender.set_fastresend(resend);
    }

    void ImKcpp::set_congestion_window_enabled(const bool state) {
        this->congestion_controller.set_congestion_window_enabled(state);
    }

    tl::expected<size_t, error> ImKcpp::set_mtu(const u32 mtu) {
        if (mtu <= constants::IKCP_OVERHEAD) {
            return tl::unexpected(error::less_than_header_size);
        }

        // TODO: Does this really need triple the size?
        this->flusher.resize(static_cast<size_t>(mtu + constants::IKCP_OVERHEAD) * 3);
        this->shared_ctx.mtu = mtu;
        this->shared_ctx.mss = this->shared_ctx.mtu - constants::IKCP_OVERHEAD;

        this->congestion_controller.set_mss(this->shared_ctx.mss);

        return mtu;
    }

    u32 ImKcpp::get_mtu() const {
        return this->shared_ctx.mtu;
    }

    u32 ImKcpp::get_max_segment_size() const {
        return this->shared_ctx.mss;
    }

    // TODO: Needs to be separate functions, and should be able to set them independently
    // TODO: Also, should return a tl::expected with the new value or an error
    void ImKcpp::set_wndsize(const u32 sndwnd, const u32 rcvwnd) {
        if (sndwnd > 0) {
            this->congestion_controller.set_send_window(sndwnd);
            this->congestion_controller.set_remote_window(sndwnd);
        }

        if (rcvwnd > 0) {
            this->congestion_controller.set_receive_window(rcvwnd);
        }
    }

    tl::expected<size_t, error> ImKcpp::peek_size() const {
        return this->receiver.peek_size();
    }

    tl::expected<size_t, error> ImKcpp::recv(const std::span<std::byte> buffer) {
        return this->receiver.recv(buffer);
    }

    size_t ImKcpp::estimate_segments_count(const size_t size) const {
        return this->sender.estimate_segments_count(size);
    }

    tl::expected<size_t, error> ImKcpp::send(const std::span<const std::byte> buffer) {
        return this->sender.send(buffer);
    }

    // TODO: Move to receiver, but only after moving snd_buf to AckController
    tl::expected<size_t, error> ImKcpp::input(std::span<const std::byte> data) {
        if (data.size() < constants::IKCP_OVERHEAD) {
            return tl::unexpected(error::less_than_header_size);
        }

        if (data.size() > this->shared_ctx.mtu) {
            return tl::unexpected(error::more_than_mtu);
        }

        const u32 prev_una = shared_ctx.snd_una;
        u32 maxack = 0, latest_ts = 0;
        bool flag = false;

        size_t offset = 0;

        while (offset + constants::IKCP_OVERHEAD <= data.size()) {
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
            this->sender.clear_acknowledged(header.una);
            this->sender.shrink_buf();

            switch (header.cmd) {
                case commands::IKCP_CMD_ACK: {
                    // TODO: Move to RtoCalculator
                    if (time_delta(this->current, header.ts) >= 0) {
                        const i32 rtt = time_delta(this->current, header.ts);
                        this->rto_calculator.update_rtt(rtt, this->interval);
                    }

                    if (this->ack_controller.should_acknowledge(header.sn)) {
                        this->sender.acknowledge_seqence_number(header.sn);
                    }

                    this->sender.shrink_buf();

                    if (!flag) {
                        flag = true;
                        maxack = header.sn;
                        latest_ts = header.ts;
                    } else {
                        if (time_delta(header.sn, maxack) > 0) {
    #ifndef IKCP_FASTACK_CONSERVE
                            maxack = header.sn;
                            latest_ts = header.ts;
    #else
                            if (time_delta(header.ts, latest_ts) > 0) {
                                maxack = sn;
                                latest_ts = ts;
                            }
    #endif
                        }
                    }

                    break;
                }
                case commands::IKCP_CMD_PUSH: {
                    if (time_delta(header.sn, shared_ctx.rcv_nxt + this->congestion_controller.get_receive_window()) < 0) {
                        this->ack_controller.emplace_back(header.sn, header.ts);
                        if (time_delta(header.sn, shared_ctx.rcv_nxt) >= 0) {
                            Segment seg;
                            seg.header = header; // TODO: Remove this copy
                            seg.data.decode_from(data, offset, header.len);

                            this->receiver.parse_data(seg);
                        }
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

        if (flag && this->ack_controller.should_acknowledge(maxack)) {
            this->sender.acknowledge_fastack(maxack, latest_ts);
        }

        congestion_controller.packet_acked(shared_ctx.snd_una, prev_una);

        return offset;
    }

    FlushResult ImKcpp::update(const u32 current, const output_callback_t& callback) {
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
            this->ts_flush += this->interval;
            if (time_delta(this->current, this->ts_flush) >= 0) {
                this->ts_flush = this->current + this->interval;
            }
            return this->flush(callback);
        }

        return {};
    }

    FlushResult ImKcpp::flush(const output_callback_t& callback) {
        // TODO: Populate result with information
        FlushResult result;

        if (!this->updated) {
            return result;
        }

        const u32 current = this->current;
        const i32 unused_receive_window = this->receiver.get_unused_receive_window();

        {
            Segment seg;
            seg.header.conv = this->shared_ctx.conv;
            seg.header.cmd = commands::IKCP_CMD_ACK;
            seg.header.frg = 0;
            seg.header.wnd = unused_receive_window;
            seg.header.una = shared_ctx.rcv_nxt;
            seg.header.sn = 0;
            seg.header.ts = 0;
            seg.header.len = 0;

            this->ack_controller.flush_acks(callback, seg);
            this->congestion_controller.update_probe_request(current);
            this->congestion_controller.flush_probes(callback, seg);
        }

        this->sender.flush_data_segments(callback, current, unused_receive_window);

        flusher.flush_if_not_empty(callback);

        return result;
    }

    u32 ImKcpp::check(const u32 current) {
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

        return current + std::min(this->interval, minimal);
    }
}