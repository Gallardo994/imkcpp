#pragma once

#include <deque>
#include "third_party/expected.hpp"

#include "types.hpp"
#include "errors.hpp"
#include "segment.hpp"
#include "results.hpp"

namespace imkcpp {
    // TODO: Benchmark against std::vector instead of std::deque
    class Receiver final {
        std::deque<Segment> rcv_buf{};

        std::deque<Segment> rcv_queue{}; // TODO: Does not need to be Segment as we don't use metadata
        u32 queue_limit = 0;

        u32 rcv_nxt = 0;

    public:
        [[nodiscard]] tl::expected<size_t, error> peek_size() const {
            if (this->rcv_queue.empty()) {
                return tl::unexpected(error::queue_empty);
            }

            const Segment& front = this->rcv_queue.front();

            if (front.header.frg == 0) {
                return front.data_size();
            }

            if (this->rcv_queue.size() < static_cast<size_t>(front.header.frg.get()) + 1) {
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
                const Fragment fragment = segment.header.frg;

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
            this->move_receive_buffer_to_queue();
        }

        void move_receive_buffer_to_queue() {
            while (!this->rcv_buf.empty()) {
                Segment& seg = this->rcv_buf.front();
                if (seg.header.sn != this->rcv_nxt || this->rcv_queue.size() >= this->queue_limit) {
                    break;
                }

                this->rcv_queue.push_back(std::move(seg));

                this->rcv_buf.pop_front();
                this->rcv_nxt++;
            }
        }

        [[nodiscard]] size_t size() const {
            return this->rcv_queue.size();
        }

        [[nodiscard]] u32 get_rcv_nxt() const {
            return this->rcv_nxt;
        }

        [[nodiscard]] bool should_receive(const u32 sn) const {
            return sn >= this->rcv_nxt;
        }

        void set_queue_limit(const u32 value) {
            this->queue_limit = value;
        }
    };
}