#pragma once

#include "types.hpp"
#include "flusher.hpp"
#include "sender_buffer.hpp"
#include "segment_tracker.hpp"
#include "utility.hpp"

namespace imkcpp {
    /// FastAckCtx is used to track the latest received segment and its timestamp.
    class FastAckCtx final {
        constexpr static u32 INVALID = 0xffffffff;

        /// The latest received segment number
        u32 maxack = INVALID;

        /// The timestamp of the latest received segment
        u32 latest_ts = INVALID;

    public:
        /// Returns true if at least one ack has been received.
        [[nodiscard]] bool is_valid() const {
            return this->maxack != INVALID;
        }

        /// Updates the latest received segment number and its timestamp.
        void update(const u32 sn, const u32 ts) {
            if (this->is_valid()) {
                if (sn > this->maxack) {
                    this->maxack = sn;
                    this->latest_ts = ts;
                }
            } else {
                this->maxack = sn;
                this->latest_ts = ts;
            }
        }

        /// Returns the latest received segment number.
        [[nodiscard]] u32 get_maxack() const {
            return this->maxack;
        }

        /// Returns the timestamp of the latest received segment.
        [[nodiscard]] u32 get_latest_ts() const {
            return this->latest_ts;
        }
    };

    template <size_t MTU>
    class AckController final {
        struct Ack {
            /// Segment number
            u32 sn = 0;

            /// Timestamp
            u32 ts = 0;

            explicit Ack() = default;
            explicit Ack(const u32 sn, const u32 ts) : sn(sn), ts(ts) { }
        };

        Flusher<MTU>& flusher;
        SenderBuffer& sender_buffer;
        SegmentTracker& segment_tracker;

        std::vector<Ack> acklist{};
        u32 rmt_una = 0;

        [[nodiscard]] bool should_acknowledge(const u32 sn) const {
            if (sn < this->segment_tracker.get_snd_una() || sn >= this->segment_tracker.get_snd_nxt()) {
                return false;
            }

            return true;
        }

    public:
        explicit AckController(Flusher<MTU>& flusher,
                               SenderBuffer& sender_buffer,
                               SegmentTracker& segment_tracker) :
                               flusher(flusher),
                               sender_buffer(sender_buffer),
                               segment_tracker(segment_tracker) {
        }

        void acknowledge_fastack(const FastAckCtx& fastack_ctx) {
            if (!fastack_ctx.is_valid()) {
                return;
            }

            const auto maxack = fastack_ctx.get_maxack();

            if (!should_acknowledge(maxack)) {
                return;
            }

            this->sender_buffer.increment_fastack_before(maxack);
        }

        /// Updates the RTO and removes acknowledged segments from the sender buffer.
        void ack_received(const u32 current, const u32 sn, const u32 ts) {
            if (this->should_acknowledge(sn)) {
                this->sender_buffer.erase(sn);
                this->sender_buffer.shrink();
            }
        }

        /**
         *Removes acknowledged segments from the sender buffer according to
         *remote una (unacknowledged segment number).
        */
        void una_received(const u32 una) {
            rmt_una = std::max(rmt_una, una);

            this->sender_buffer.erase_before(una);
            this->sender_buffer.shrink();
        }

        /// Adds a segment to the acknowledgement list to be sent later.
        void schedule_ack(const u32 sn, const u32 ts) {
            this->acklist.emplace_back(sn, ts);
        }

        /// Reserves space in the acknowledgement vector.
        void reserve(const size_t size) {
            this->acklist.reserve(size);
        }

        /// Sends all scheduled acknowledgements and clears the acknowledgement list.
        void flush_acks(FlushResult& flush_result, const output_callback_t& output, Segment& base_segment) {
            size_t count = 0;

            for (const Ack& ack : this->acklist) {
                if (ack.sn < this->rmt_una) {
                    continue;
                }

                flush_result.total_bytes_sent += this->flusher.flush_if_full(output);

                base_segment.header.sn = ack.sn;
                base_segment.header.ts = ack.ts;

                this->flusher.emplace_segment(base_segment);

                ++count;
            }

            flush_result.cmd_ack_count += count;

            this->acklist.clear();
        }
    };
}