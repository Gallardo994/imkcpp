#pragma once

#include <deque>
#include "third_party/expected.hpp"

#include "types.hpp"
#include "errors.hpp"
#include "segment.hpp"
#include "congestion_controller.hpp"
#include "receiver_buffer.hpp"
#include "utility.hpp"

namespace imkcpp {
    // TODO: Benchmark against std::vector instead of std::deque
    template <size_t MTU>
    class Receiver final {
        constexpr static size_t MAX_SEGMENT_SIZE = MTU_TO_MSS<MTU>();

        ReceiverBuffer<MTU>& receiver_buffer;
        CongestionController<MTU>& congestion_controller;

        std::deque<Segment> rcv_queue{}; // TODO: Does not need to be Segment as we don't use metadata

    public:

        explicit Receiver(ReceiverBuffer<MTU>& receiver_buffer,
                          CongestionController<MTU>& congestion_controller) :
                          receiver_buffer(receiver_buffer),
                          congestion_controller(congestion_controller) {}

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

        tl::expected<size_t, error> recv(const std::span<std::byte> buffer) {
            const auto peeksize = this->peek_size();

            if (!peeksize.has_value()) {
                return tl::unexpected(peeksize.error());
            }

            if (peeksize.value() > buffer.size()) {
                return tl::unexpected(error::buffer_too_small);
            }

            const bool recover = this->rcv_queue.size() >= this->congestion_controller.get_receive_window();

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

            if (this->congestion_controller.get_receive_window() > this->rcv_queue.size() && recover) {
                this->congestion_controller.set_probe_flag(constants::IKCP_ASK_TELL);
            }

            return offset;
        }

        void move_receive_buffer_to_queue() {
            this->receiver_buffer.move_receive_buffer_to_queue(this->rcv_queue);
        }

        // TODO: This has to be a u16 as in SegmentHeader
        [[nodiscard]] i32 get_unused_receive_window() const {
            return std::max(static_cast<i32>(this->congestion_controller.get_receive_window()) - static_cast<i32>(this->rcv_queue.size()), 0);
        }
    };
}