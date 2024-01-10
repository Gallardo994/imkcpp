#pragma once

#include <deque>
#include "types.hpp"
#include "segment.hpp"
#include "shared_ctx.hpp"
#include "segment_tracker.hpp"

namespace imkcpp {
    class SenderBuffer {
        SharedCtx& shared_ctx;
        SegmentTracker& segment_tracker;

        std::deque<Segment> snd_buf{};

    public:
        explicit SenderBuffer(SharedCtx& shared_ctx,
                              SegmentTracker& segment_tracker) :
                              shared_ctx(shared_ctx),
                              segment_tracker(segment_tracker) {}

        void push_segment(Segment& segment) {
            this->snd_buf.push_back(std::move(segment));
        }

        void shrink() {
            if (!this->snd_buf.empty()) {
                const Segment& seg = this->snd_buf.front();
                this->segment_tracker.set_snd_una(seg.header.sn);
            } else {
                this->segment_tracker.reset_snd_una();
            }
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
            for (const auto it = this->snd_buf.begin(); it != this->snd_buf.end();) {
                if (it->header.sn < sn) {
                    it->metadata.fastack++;
                } else {
                    break;
                }
            }
        }

        // Returns nearest delta from current time to the earliest resend time of a segment in the buffer.
        // If there are no segments in the buffer, returns std::nullopt
        [[nodiscard]] std::optional<u32> get_earliest_transmit_delta(const u32 current) const {
            if (this->snd_buf.empty()) {
                return std::nullopt;
            }

            constexpr u32 default_value = std::numeric_limits<u32>::max();
            u32 tm_packet = default_value;

            for (const Segment& seg : this->snd_buf) {
                if (seg.metadata.resendts <= current) {
                    return 0;
                }

                tm_packet = std::min<u32>(tm_packet, seg.metadata.resendts - current);
            }

            if (tm_packet == default_value) {
                return std::nullopt;
            }

            return tm_packet;
        }

        [[nodiscard]] bool empty() const {
            return this->snd_buf.empty();
        }

        // TODO: Get rid of this
        std::deque<Segment>& get() {
            return this->snd_buf;
        }
    };
}