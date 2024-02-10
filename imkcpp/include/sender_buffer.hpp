#pragma once

#include <deque>
#include <limits>

#include "types.hpp"
#include "clock.hpp"
#include "segment.hpp"

namespace imkcpp {
    class SenderBuffer final {
        std::deque<Segment> snd_buf{};

    public:
        std::deque<Segment>::iterator begin() { return snd_buf.begin(); }
        std::deque<Segment>::iterator end() { return snd_buf.end(); }

        void push_segment(Segment& segment) {
            this->snd_buf.push_back(std::move(segment));
        }

        [[nodiscard]] size_t size() const {
            return this->snd_buf.size();
        }

        [[nodiscard]] std::optional<u32> get_first_sequence_number_in_flight() const {
            if (!this->snd_buf.empty()) {
                const Segment& seg = this->snd_buf.front();
                return seg.header.sn;
            }

            return std::nullopt;
        }

        void erase(const u32 sn) {
            for (auto it = this->snd_buf.begin(); it != this->snd_buf.end();) {
                if (sn == it->header.sn) {
                    this->snd_buf.erase(it);
                    break;
                }

                if (sn < it->header.sn) {
                    break;
                }

                ++it;
            }
        }

        void erase_before(const u32 sn) {
            for (auto it = this->snd_buf.begin(); it != this->snd_buf.end();) {
                if (sn > it->header.sn) {
                    it = this->snd_buf.erase(it);
                } else {
                    break;
                }
            }
        }

        void increment_fastack_before(const u32 sn) {
            for (Segment& seg : this->snd_buf) {
                if (seg.header.sn < sn) {
                    seg.metadata.fastack++;
                } else {
                    break;
                }
            }
        }

        /**
         *Returns nearest delta from current time to the earliest resend time of a segment in the buffer.
         *If there are no segments in the buffer, returns std::nullopt
         */
        [[nodiscard]] std::optional<duration_t> get_earliest_transmit_delta(const timepoint_t current) const {
            if (snd_buf.empty()) {
                return std::nullopt;
            }

            auto earliest_delta = std::optional<duration_t>{};

            for (const auto& seg : snd_buf) {
                if (seg.metadata.resendts <= current) {
                    return duration_t{0};
                }

                const auto delta = seg.metadata.resendts - current;

                if (!earliest_delta || delta < earliest_delta.value()) {
                    earliest_delta = delta;
                }
            }

            return earliest_delta;
        }

        [[nodiscard]] bool empty() const {
            return this->snd_buf.empty();
        }
    };
}