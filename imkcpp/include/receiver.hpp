#pragma once

#include <deque>
#include "third_party/expected.hpp"

#include "types.hpp"
#include "errors.hpp"
#include "segment.hpp"
#include "congestion_controller.hpp"

namespace imkcpp {
    class Receiver {
        SharedCtx& shared_ctx;
        CongestionController& congestion_controller;

        std::deque<Segment> rcv_queue{};
        std::deque<Segment> rcv_buf{};
    public:

        explicit Receiver(SharedCtx& shared_ctx, CongestionController& congestion_controller) : shared_ctx(shared_ctx), congestion_controller(congestion_controller) {}

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
            while (!this->rcv_buf.empty()) {
                Segment& seg = this->rcv_buf.front();
                if (seg.header.sn != shared_ctx.rcv_nxt || this->rcv_queue.size() >= this->congestion_controller.get_receive_window()) {
                    break;
                }

                this->rcv_queue.push_back(std::move(seg));
                this->rcv_buf.pop_front();
                this->shared_ctx.rcv_nxt++;
            }
        }

        void parse_data(const Segment& newseg) {
            u32 sn = newseg.header.sn;
            bool repeat = false;

            // TODO: Partially duplicates logic in imkcpp.hpp. Should probably stay here
            if (sn >= this->shared_ctx.rcv_nxt + this->congestion_controller.get_receive_window() || sn < this->shared_ctx.rcv_nxt) {
                return;
            }

            const auto it = std::find_if(this->rcv_buf.rbegin(), this->rcv_buf.rend(), [sn](const Segment& seg) {
                return time_delta(sn, seg.header.sn) <= 0;
            });

            if (it != this->rcv_buf.rend() && it->header.sn == sn) {
                repeat = true;
            }

            if (!repeat) {
                // TODO: This copy is probably redundant
                this->rcv_buf.insert(it.base(), newseg);
            }

            this->move_receive_buffer_to_queue();
        }

        [[nodiscard]] i32 get_unused_receive_window() const {
            return std::max(static_cast<i32>(this->congestion_controller.get_receive_window()) - static_cast<i32>(this->rcv_queue.size()), 0);
        }
    };
}