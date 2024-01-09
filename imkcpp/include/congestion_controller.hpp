#pragma once

#include "types.hpp"
#include "constants.hpp"

namespace imkcpp {
    class CongestionController {
        bool congestion_window = true;
        u32 mss = constants::IKCP_MTU_DEF - constants::IKCP_OVERHEAD;

        u32 rcv_wnd = constants::IKCP_WND_RCV;
        u32 rmt_wnd = constants::IKCP_WND_SND;
        u32 snd_wnd = constants::IKCP_WND_SND;

        u32 ssthresh = constants::IKCP_THRESH_INIT;
        u32 cwnd = 0;
        u32 incr = 0;

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
    };
}