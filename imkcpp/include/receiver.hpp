#pragma once

#include <deque>
#include "third_party/expected.hpp"

#include "types.hpp"
#include "errors.hpp"
#include "segment.hpp"
#include "receiver_buffer.hpp"
#include "results.hpp"

namespace imkcpp {
    // TODO: Benchmark against std::vector instead of std::deque
    class Receiver final {
        SegmentTracker& segment_tracker;
        ReceiverBuffer& receiver_buffer;

        std::deque<Segment> rcv_queue{}; // TODO: Does not need to be Segment as we don't use metadata

    public:

        explicit Receiver(SegmentTracker& segment_tracker, ReceiverBuffer& receiver_buffer) :
            segment_tracker(segment_tracker),
            receiver_buffer(receiver_buffer) {}

        [[nodiscard]] tl::expected<size_t, error> peek_size() const {
            if (this->rcv_queue.empty()) {
                return tl::unexpected(error::queue_empty);
            }

            const Segment& front = this->rcv_queue.front();

            if (front.header.frg == 0) {
                return front.data_size();
            }

            if (this->rcv_queue.size() < static_cast<size_t>(front.header.frg) + 1) {
                return tl::unexpected(error::waiting_for_fragment);
            }

            size_t length = 0;
            for (const Segment& seg : this->rcv_queue) {
                length += seg.data_size();

                if (seg.header.frg == 0) {
                    break;
                }
            }

            return length;
        }

        tl::expected<ReceiveResult, error> recv(const std::span<std::byte> buffer, const u32 rcv_wnd) {
            const auto peeksize = this->peek_size();

            if (!peeksize.has_value()) {
                return tl::unexpected(peeksize.error());
            }

            if (peeksize.value() > buffer.size()) {
                return tl::unexpected(error::buffer_too_small);
            }

            const bool is_full = this->rcv_queue.size() >= rcv_wnd;

            size_t offset = 0;
            for (auto it = this->rcv_queue.begin(); it != this->rcv_queue.end();) {
                Segment& segment = *it;
                const u8 fragment = segment.header.frg;

                const size_t copy_len = std::min(segment.data_size(), buffer.size() - offset);
                segment.data.encode_to(buffer, offset, copy_len);

                it = this->rcv_queue.erase(it);

                if (fragment == 0) {
                    break;
                }

                if (offset >= peeksize.value()) {
                    break;
                }
            }

            assert(offset == peeksize);

            this->move_receive_buffer_to_queue();

            const ReceiveResult result{
                .size = offset,
                .recovered = rcv_wnd > this->rcv_queue.size() && is_full,
            };

            return result;
        }

        void move_receive_buffer_to_queue() {
            const u32 queue_limit = this->receiver_buffer.get_queue_limit();

            while (!this->receiver_buffer.empty()) {
                Segment& seg = this->receiver_buffer.front();
                if (seg.header.sn != this->segment_tracker.get_rcv_nxt() || this->rcv_queue.size() >= queue_limit) {
                    break;
                }

                this->rcv_queue.push_back(std::move(seg));

                this->receiver_buffer.pop_front();
                this->segment_tracker.increment_rcv_nxt();
            }
        }

        [[nodiscard]] size_t size() const {
            return this->rcv_queue.size();
        }
    };
}