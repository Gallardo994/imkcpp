#pragma once

#include "types.hpp"
#include "constants.hpp"
#include "utility.hpp"

namespace imkcpp {
    class CongestionController {
        bool congestion_window = true;
        u32 mss = constants::IKCP_MTU_DEF - constants::IKCP_OVERHEAD;

        u32 rcv_wnd = constants::IKCP_WND_RCV; // Receive Window
        u32 rmt_wnd = constants::IKCP_WND_SND; // Remote Window (receive window for the other side, advertised by the other side)
        u32 snd_wnd = constants::IKCP_WND_SND; // Send Window

        u32 ssthresh = constants::IKCP_THRESH_INIT; // Slow Start Threshold
        u32 cwnd = 0; // Congestion Window
        u32 incr = 0; // Increment

        // Probe is a part of flow control but it's deeply intertwined with congestion control so it's here

        u32 probe = 0; // Probing
        u32 ts_probe = 0; // Timestamp of the last time we probed the remote window
        u32 probe_wait = 0; // How long we should wait before probing again

    public:
        void set_mss(const u32 mss) {
            this->mss = mss;
        }

        void set_congestion_window_enabled(const bool state) {
            this->congestion_window = state;
        }

        void set_receive_window(const u32 rcv_wnd) {
            this->rcv_wnd = std::max(rcv_wnd, constants::IKCP_WND_RCV);
        }

        [[nodiscard]] u32 get_receive_window() const {
            return this->rcv_wnd;
        }

        void set_remote_window(const u32 rmt_wnd) {
            this->rmt_wnd = rmt_wnd;
        }

        [[nodiscard]] u32 get_remote_window() const {
            return this->rmt_wnd;
        }

        void set_send_window(const u32 snd_wnd) {
            this->snd_wnd = snd_wnd;
        }

        [[nodiscard]] u32 get_send_window() const {
            return this->snd_wnd;
        }

        void resent(const u32 packets_in_flight, const u32 resent) {
            this->ssthresh = std::max(packets_in_flight / 2, constants::IKCP_THRESH_MIN);
            this->cwnd = this->ssthresh + resent;
            this->incr = this->cwnd * this->mss;
        }

        void packet_lost() {
            this->ssthresh = std::max(this->cwnd / 2, constants::IKCP_THRESH_MIN);
            this->cwnd = 1;
            this->incr = this->mss;
        }

        void packet_acked(const u32 latest_una, const u32 prev_una) {
            if (latest_una > prev_una) {
                if (this->cwnd < this->rmt_wnd) {
                    const u32 mss = this->mss;

                    if (this->cwnd < this->ssthresh) {
                        this->cwnd++;
                        this->incr += mss;
                    } else {
                        if (this->incr < mss) {
                            this->incr = mss;
                        }

                        this->incr += (mss * mss) / this->incr + (mss / 16);

                        if ((this->cwnd + 1) * mss <= this->incr) {
                            this->cwnd = (this->incr + mss - 1) / ((mss > 0) ? mss : 1);
                        }
                    }

                    if (this->cwnd > this->rmt_wnd) {
                        this->cwnd = this->rmt_wnd;
                        this->incr = this->rmt_wnd * mss;
                    }
                }
            }
        }

        void ensure_at_least_one_packet_in_flight() {
            if (this->cwnd < 1) {
                this->cwnd = 1;
                this->incr = this->mss;
            }
        }

        [[nodiscard]] u32 calculate_congestion_window() const {
            u32 cwnd = std::min(this->snd_wnd, this->rmt_wnd);

            if (this->congestion_window) {
                cwnd = std::min(this->cwnd, cwnd);
            }

            return cwnd;
        }

        // Flow control mechanism

        void update_probe_request(const u32 current) {
            if (this->rmt_wnd != 0) {
                this->ts_probe = 0;
                this->probe_wait = 0;
            }

            if (this->probe_wait == 0) {
                this->probe_wait = constants::IKCP_PROBE_INIT;
                this->ts_probe = current + this->probe_wait;
            } else {
                if (time_delta(current, this->ts_probe) >= 0) {
                    if (this->probe_wait < constants::IKCP_PROBE_INIT) {
                        this->probe_wait = constants::IKCP_PROBE_INIT;
                    }

                    this->probe_wait += this->probe_wait / 2;
                    if (this->probe_wait > constants::IKCP_PROBE_LIMIT) {
                        this->probe_wait = constants::IKCP_PROBE_LIMIT;
                    }
                    this->ts_probe = current + this->probe_wait;
                    this->set_probe_flag(constants::IKCP_ASK_SEND);
                }
            }
        }

        void set_probe_flag(const u32 flag) {
            this->probe |= flag;
        }

        [[nodiscard]] bool has_probe_flag(const u32 flag) const {
            return (this->probe & flag) != 0;
        }

        void reset_probe_flags() {
            this->probe = 0;
        }
    };
}