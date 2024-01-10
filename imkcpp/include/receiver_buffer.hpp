#pragma once

#include <deque>
#include "types.hpp"
#include "segment.hpp"
#include "shared_ctx.hpp"
#include "segment_tracker.hpp"

namespace imkcpp {
    class ReceiverBuffer {
        SharedCtx& shared_ctx;
        CongestionController& congestion_controller;
        SegmentTracker& segment_tracker;

        std::deque<Segment> rcv_buf{};

    public:
        explicit ReceiverBuffer(SharedCtx& shared_ctx,
                                CongestionController& congestion_controller,
                                SegmentTracker& segment_tracker) :
                                shared_ctx(shared_ctx),
                                congestion_controller(congestion_controller),
                                segment_tracker(segment_tracker) {}

        void emplace_segment(const Segment& newseg) {
            u32 sn = newseg.header.sn;
            bool repeat = false;

            // TODO: Partially duplicates logic in imkcpp.hpp. Should probably stay here
            const auto rcv_nxt = this->segment_tracker.get_rcv_nxt();
            if (sn >= rcv_nxt + this->congestion_controller.get_receive_window() || sn < rcv_nxt) {
                return;
            }

            const auto it = std::find_if(this->rcv_buf.rbegin(), this->rcv_buf.rend(), [sn](const Segment& seg) {
                return sn <= seg.header.sn;
            });

            if (it != this->rcv_buf.rend() && it->header.sn == sn) {
                repeat = true;
            }

            if (!repeat) {
                // TODO: This copy is probably redundant
                this->rcv_buf.insert(it.base(), newseg);
            }
        }

        void move_receive_buffer_to_queue(std::deque<Segment>& rcv_queue) {
            while (!this->rcv_buf.empty()) {
                Segment& seg = rcv_buf.front();
                if (seg.header.sn != segment_tracker.get_rcv_nxt() || rcv_queue.size() >= this->congestion_controller.get_receive_window()) {
                    break;
                }

                rcv_queue.push_back(std::move(seg));

                this->rcv_buf.pop_front();
                this->segment_tracker.increment_rcv_nxt();
            }
        }
    };
}