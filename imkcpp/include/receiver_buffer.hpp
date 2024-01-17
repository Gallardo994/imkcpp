#pragma once

#include <deque>
#include "types.hpp"
#include "segment.hpp"
#include "segment_tracker.hpp"

namespace imkcpp {
    class ReceiverBuffer final {
        SegmentTracker& segment_tracker;

        std::deque<Segment> rcv_buf{}; // TODO: Does not need to be Segment as we don't use metadata
        u32 queue_limit = 0;

    public:
        explicit ReceiverBuffer(SegmentTracker& segment_tracker) : segment_tracker(segment_tracker) {}

        void set_queue_limit(const u32 rcv_wnd) {
            this->queue_limit = rcv_wnd;
        }

        void emplace_segment(const SegmentHeader& header, SegmentData& data) {
            u32 sn = header.sn;

            assert(this->segment_tracker.should_receive(sn));

            const auto it = std::find_if(this->rcv_buf.rbegin(), this->rcv_buf.rend(), [sn](const Segment& seg) {
                return sn >= seg.header.sn;
            });

            if (it == this->rcv_buf.rend() || it->header.sn != sn) {
                this->rcv_buf.emplace(it.base(), header, data);
            }
        }

        void move_receive_buffer_to_queue(std::deque<Segment>& rcv_queue) {
            while (!this->rcv_buf.empty()) {
                Segment& seg = rcv_buf.front();
                if (seg.header.sn != segment_tracker.get_rcv_nxt() || rcv_queue.size() >= this->queue_limit) {
                    break;
                }

                rcv_queue.push_back(std::move(seg));

                this->rcv_buf.pop_front();
                this->segment_tracker.increment_rcv_nxt();
            }
        }
    };
}