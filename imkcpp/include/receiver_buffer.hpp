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

        void set_queue_limit(const u32 value) {
            this->queue_limit = value;
        }

        void emplace_segment(const SegmentHeader& header, SegmentData& data) {
            u32 sn = header.sn;

            assert(this->segment_tracker.should_receive(sn));

            const auto rit = std::find_if(this->rcv_buf.rbegin(), this->rcv_buf.rend(), [sn](const Segment& seg) {
                return seg.header.sn < sn;
            });

            const auto it = rit.base();

            if (it != this->rcv_buf.end() && it->header.sn == sn) {
                return;
            }

            this->rcv_buf.emplace(it, header, data);
        }

        size_t move_receive_buffer_to_queue(std::deque<Segment>& rcv_queue) {
            size_t count = 0;

            while (!this->rcv_buf.empty()) {
                Segment& seg = rcv_buf.front();
                if (seg.header.sn != segment_tracker.get_rcv_nxt() || rcv_queue.size() >= this->queue_limit) {
                    break;
                }

                rcv_queue.push_back(std::move(seg));

                this->rcv_buf.pop_front();
                this->segment_tracker.increment_rcv_nxt();

                count++;
            }

            return count;
        }
    };
}