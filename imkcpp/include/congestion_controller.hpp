#pragma once


#include "types.hpp"
#include "constants.hpp"
#include "utility.hpp"
#include "segment_tracker.hpp"

namespace imkcpp {
    template <size_t MTU>
    class CongestionController final {
        constexpr static size_t MAX_SEGMENT_SIZE = MTU_TO_MSS<MTU>();

        SegmentTracker& segment_tracker;

        bool congestion_window = true; // Congestion Window Enabled

        u32 rcv_wnd = constants::IKCP_WND_RCV; // Receive Window
        u32 rmt_wnd = constants::IKCP_WND_SND; // Remote Window (receive window for the other side, advertised by the other side)
        u32 snd_wnd = constants::IKCP_WND_SND; // Send Window

        u32 ssthresh = constants::IKCP_THRESH_INIT; // Slow Start Threshold
        u32 cwnd = 0; // Congestion Window
        u32 incr = 0; // Increment

        // Probe is a part of flow control but it's deeply intertwined with congestion control so it's here

        u32 probe = 0; // Probe flags
        u32 ts_probe = 0; // Timestamp of the last time we probed the remote window
        u32 probe_wait = 0; // How long we should wait before probing again

    public:
        explicit CongestionController(SegmentTracker& segment_tracker) :
                                      segment_tracker(segment_tracker) {}

        void set_congestion_window_enabled(const bool state) {
            this->congestion_window = state;
        }

        void set_receive_window(const u32 rcv_wnd) {
            this->rcv_wnd = std::max(rcv_wnd, constants::IKCP_WND_RCV);
        }

        [[nodiscard]] u32 get_receive_window() const {
            return this->rcv_wnd;
        }

        [[nodiscard]] bool fits_receive_window(const u32 sn) const {
            return sn < this->segment_tracker.get_rcv_nxt() + this->get_receive_window();
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

        // Closely resembles TCP Reno algorithm.
        void packets_resent(const u32 packets_in_flight, const u32 resent) {
            this->ssthresh = std::max(packets_in_flight / 2, constants::IKCP_THRESH_MIN);
            this->cwnd = this->ssthresh + resent;
            this->incr = this->cwnd * MAX_SEGMENT_SIZE;
        }

        void packet_lost() {
            this->ssthresh = std::max(this->cwnd / 2, constants::IKCP_THRESH_MIN);
            this->cwnd = 1;
            this->incr = MAX_SEGMENT_SIZE;
        }

        void adjust_parameters() {
            if (this->cwnd < this->rmt_wnd) {
                if (this->cwnd < this->ssthresh) {
                    ++this->cwnd;
                    this->incr += MAX_SEGMENT_SIZE;
                } else {
                    if (this->incr < MAX_SEGMENT_SIZE) {
                        this->incr = MAX_SEGMENT_SIZE;
                    }

                    this->incr += (MAX_SEGMENT_SIZE * MAX_SEGMENT_SIZE) / this->incr + (MAX_SEGMENT_SIZE / 16);

                    if ((this->cwnd + 1) * MAX_SEGMENT_SIZE <= this->incr) {
                        this->cwnd = (this->incr + MAX_SEGMENT_SIZE - 1) / MAX_SEGMENT_SIZE;
                    }
                }

                if (this->cwnd > this->rmt_wnd) {
                    this->cwnd = this->rmt_wnd;
                    this->incr = this->rmt_wnd * MAX_SEGMENT_SIZE;
                }
            }
        }

        void ensure_at_least_one_packet_in_flight() {
            if (this->cwnd < 1) {
                this->cwnd = 1;
                this->incr = MAX_SEGMENT_SIZE;
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