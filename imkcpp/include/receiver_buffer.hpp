#pragma once

#include <deque>
#include "types.hpp"
#include "segment.hpp"

namespace imkcpp {
    class ReceiverBuffer final {

        std::deque<Segment> rcv_buf{}; // TODO: Does not need to be Segment as we don't use metadata
        u32 queue_limit = 0;

    public:
        void set_queue_limit(const u32 value) {
            this->queue_limit = value;
        }

        [[nodiscard]] u32 get_queue_limit() const {
            return this->queue_limit;
        }

        [[nodiscard]] bool empty() const {
            return this->rcv_buf.empty();
        }

        [[nodiscard]] Segment& front() {
            return this->rcv_buf.front();
        }

        [[nodiscard]] size_t size() const {
            return this->rcv_buf.size();
        }

        void pop_front() {
            this->rcv_buf.pop_front();
        }

        void emplace_segment(const SegmentHeader& header, SegmentData& data) {
            u32 sn = header.sn;

            const auto rit = std::find_if(this->rcv_buf.rbegin(), this->rcv_buf.rend(), [sn](const Segment& seg) {
                return seg.header.sn < sn;
            });

            const auto it = rit.base();

            if (it != this->rcv_buf.end() && it->header.sn == sn) {
                return;
            }

            this->rcv_buf.emplace(it, header, data);
        }
    };
}