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
        timepoint_t latest_ts;

    public:
        /// Returns true if at least one ack has been received.
        [[nodiscard]] bool is_valid() const {
            return this->maxack != INVALID;
        }

        /// Updates the latest received segment number and its timestamp.
        void update(const u32 sn, const timepoint_t ts) {
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
        [[nodiscard]] timepoint_t get_latest_ts() const {
            return this->latest_ts;
        }
    };

    struct Ack {
        /// Segment number
        u32 sn = 0;

        /// Timestamp
        timepoint_t ts;

        explicit Ack() = default;
        explicit Ack(const u32 sn, const timepoint_t ts) : sn(sn), ts(ts) { }
    };

    class AckController final {
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
        explicit AckController(SenderBuffer& sender_buffer,
                               SegmentTracker& segment_tracker) :
                               sender_buffer(sender_buffer),
                               segment_tracker(segment_tracker) {
        }

        [[nodiscard]] std::vector<Ack>::const_iterator begin() { return acklist.begin(); }
        [[nodiscard]] std::vector<Ack>::const_iterator end() { return acklist.end(); }

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

        void update_remote_una() {
            if (const std::optional<u32> first_sn = this->sender_buffer.get_first_sequence_number_in_flight(); first_sn.has_value()) {
                this->segment_tracker.set_snd_una(first_sn.value());
            } else {
                this->segment_tracker.reset_snd_una();
            }
        }

        /// Removes acknowledged segments from the sender buffer.
        void ack_received(const u32 sn) {
            if (this->should_acknowledge(sn)) {
                this->sender_buffer.erase(sn);
                this->update_remote_una();
            }
        }

        /**
         *Removes acknowledged segments from the sender buffer according to
         *remote una (unacknowledged segment number).
        */
        void una_received(const u32 una) {
            rmt_una = std::max(rmt_una, una);

            this->sender_buffer.erase_before(una);
            this->update_remote_una();
        }

        /// Adds a segment to the acknowledgement list to be sent later.
        void schedule_ack(const u32 sn, const timepoint_t ts) {
            this->acklist.emplace_back(sn, ts);
        }

        /// Reserves space in the acknowledgement vector.
        void reserve(const size_t size) {
            this->acklist.reserve(size);
        }

        /// Clears the acknowledgement list.
        void clear() {
            this->acklist.clear();
        }

        [[nodiscard]] bool empty() const {
            return this->acklist.empty();
        }

        [[nodiscard]] size_t size() const {
            return this->acklist.size();
        }
    };
}