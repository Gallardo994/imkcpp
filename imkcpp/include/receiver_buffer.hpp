#pragma once

#include <deque>
#include "types.hpp"
#include "segment.hpp"
#include "segment_tracker.hpp"
#include "utility.hpp"

namespace imkcpp {
    template <size_t MTU>
    class ReceiverBuffer {
        constexpr static size_t MAX_SEGMENT_SIZE = MTU_TO_MSS<MTU>();

        CongestionController<MTU>& congestion_controller;
        SegmentTracker& segment_tracker;

        std::deque<Segment> rcv_buf{};

    public:
        explicit ReceiverBuffer(CongestionController<MTU>& congestion_controller,
                                SegmentTracker& segment_tracker) :
                                congestion_controller(congestion_controller),
                                segment_tracker(segment_tracker) {}

        void emplace_segment(const SegmentHeader& header, SegmentData& data) {
            u32 sn = header.sn;
            bool repeat = false;

            assert(this->segment_tracker.should_receive(sn));
            assert(this->congestion_controller.fits_receive_window(sn));

            const auto it = std::find_if(this->rcv_buf.rbegin(), this->rcv_buf.rend(), [sn](const Segment& seg) {
                return sn <= seg.header.sn;
            });

            if (it != this->rcv_buf.rend() && it->header.sn == sn) {
                repeat = true;
            }

            if (!repeat) {
                this->rcv_buf.emplace(it.base(), header, data);
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