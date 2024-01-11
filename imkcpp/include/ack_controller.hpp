#pragma once

#include "types.hpp"
#include "flusher.hpp"
#include "rto_calculator.hpp"
#include "sender_buffer.hpp"
#include "segment_tracker.hpp"
#include "utility.hpp"

namespace imkcpp {
    // FastAckCtx is used to track the latest received segment and its timestamp.
    class FastAckCtx {
        bool valid = false; // At least one ack has been received
        u32 maxack = 0; // The latest received segment number
        u32 latest_ts = 0; // The timestamp of the latest received segment

    public:
        // Returns true if at least one ack has been received.
        [[nodiscard]] bool is_valid() const {
            return this->valid;
        }

        // Updates the latest received segment number and its timestamp.
        void update(const u32 sn, const u32 ts) {
            if (this->valid) {
                if (sn > this->maxack) {
                    this->maxack = sn;
                    this->latest_ts = ts;
                }
            } else {
                this->valid = true;
                this->maxack = sn;
                this->latest_ts = ts;
            }
        }

        // Returns the latest received segment number.
        [[nodiscard]] u32 get_maxack() const {
            return this->maxack;
        }

        // Returns the timestamp of the latest received segment.
        [[nodiscard]] u32 get_latest_ts() const {
            return this->latest_ts;
        }
    };

    template <size_t MTU>
    class AckController {
        struct Ack {
            u32 sn; // Segment number
            u32 ts; // Timestamp

            explicit Ack() : sn(0), ts(0) { }
            explicit Ack(const u32 sn, const u32 ts) : sn(sn), ts(ts) { }
        };

        Flusher<MTU>& flusher;
        RtoCalculator& rto_calculator;
        SenderBuffer& sender_buffer;
        SegmentTracker& segment_tracker;

        std::vector<Ack> acklist{};

        [[nodiscard]] bool should_acknowledge(const u32 sn) const {
            if (sn < this->segment_tracker.get_snd_una() || sn >= this->segment_tracker.get_snd_nxt()) {
                return false;
            }

            return true;
        }

    public:
        explicit AckController(Flusher<MTU>& flusher,
                               RtoCalculator& rto_calculator,
                               SenderBuffer& sender_buffer,
                               SegmentTracker& segment_tracker) :
                               flusher(flusher),
                               rto_calculator(rto_calculator),
                               sender_buffer(sender_buffer),
                               segment_tracker(segment_tracker) {}

        void acknowledge_fastack(const FastAckCtx& fast_ack_ctx) {
            if (!fast_ack_ctx.is_valid()) {
                return;
            }

            const auto maxack = fast_ack_ctx.get_maxack();

            if (!should_acknowledge(maxack)) {
                return;
            }

            this->sender_buffer.increment_fastack_before(maxack);
        }

        // Updates the RTO and removes acknowledged segments from the sender buffer.
        void ack_received(const u32 current, const u32 sn, const u32 ts) {
            if (const i32 rtt = time_delta(current, ts); rtt >= 0) {
                this->rto_calculator.update_rtt(rtt);
            }

            if (this->should_acknowledge(sn)) {
                this->sender_buffer.erase(sn);
            }

            this->sender_buffer.shrink();
        }

        // Removes acknowledged segments from the sender buffer according to
        // remote una (unacknowledged segment number).
        void una_received(const u32 una) {
            this->sender_buffer.erase_before(una);
            this->sender_buffer.shrink();
        }

        // Adds a segment to the acknowledgement list to be sent later.
        void schedule_ack(const u32 sn, const u32 ts) {
            this->acklist.emplace_back(sn, ts);
        }

        // Sends all scheduled acknowledgements and clears the acknowledgement list.
        void flush_acks(FlushResult& flush_result, const output_callback_t& output, Segment& base_segment) {
            for (const Ack& ack : this->acklist) {
                flush_result.total_bytes_sent += this->flusher.flush_if_full(output);

                base_segment.header.sn = ack.sn;
                base_segment.header.ts = ack.ts;

                this->flusher.emplace_segment(base_segment);
            }

            flush_result.cmd_ack_count += this->acklist.size();

            this->acklist.clear();
        }
    };
}