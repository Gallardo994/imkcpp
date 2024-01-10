#pragma once

#include "types.hpp"
#include "flusher.hpp"
#include "rto_calculator.hpp"
#include "sender_buffer.hpp"

namespace imkcpp {
    class FastAckCtx {
        bool valid = false;
        u32 maxack = 0;
        u32 latest_ts = 0;

    public:
        [[nodiscard]] bool is_valid() const {
            return this->valid;
        }

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

        [[nodiscard]] u32 get_maxack() const {
            return this->maxack;
        }

        [[nodiscard]] u32 get_latest_ts() const {
            return this->latest_ts;
        }
    };

    class AckController {
        struct Ack {
            u32 sn, ts;

            explicit Ack() : sn(0), ts(0) { }
            explicit Ack(const u32 sn, const u32 ts) : sn(sn), ts(ts) { }
        };

        SharedCtx& shared_ctx;
        RtoCalculator& rto_calculator;
        Flusher& flusher;
        SenderBuffer& sender_buffer;

        std::vector<Ack> acklist{};

    public:
        explicit AckController(Flusher& flusher,
                               RtoCalculator& rto_calculator,
                               SenderBuffer& sender_buffer,
                               SharedCtx& shared_ctx) :
                               shared_ctx(shared_ctx),
                               rto_calculator(rto_calculator),
                               flusher(flusher),
                               sender_buffer(sender_buffer) {}

        [[nodiscard]] bool should_acknowledge(const u32 sn) const {
            if (sn < shared_ctx.snd_una || sn >= shared_ctx.snd_nxt) {
                return false;
            }

            return true;
        }

        void acknowledge_fastack(const FastAckCtx& fast_ack_ctx) {
            if (!fast_ack_ctx.is_valid()) {
                return;
            }

            const auto maxack = fast_ack_ctx.get_maxack();

            if (!should_acknowledge(maxack)) {
                return;
            }

            auto& snd_buf = this->sender_buffer.get();

            for (Segment& seg : snd_buf) {
                if (maxack < seg.header.sn) {
                    break;
                }

                if (maxack != seg.header.sn) {
                    seg.metadata.fastack++;
                }
            }
        }

        void ack_received(const u32 current, const u32 sn, const u32 ts) {
            if (const i32 rtt = time_delta(current, ts); rtt >= 0) {
                this->rto_calculator.update_rtt(rtt);
            }

            if (this->should_acknowledge(sn)) {
                this->sender_buffer.erase(sn);
            }

            this->sender_buffer.shrink();
        }

        void una_received(const u32 una) {
            this->sender_buffer.erase_before(una);
            this->sender_buffer.shrink();
        }

        void emplace_back(const u32 sn, const u32 ts) {
            this->acklist.emplace_back(sn, ts);
        }

        void flush_acks(const output_callback_t& output, Segment& base_segment) {
            // TODO: This is not optimal to send IKCP_OVERHEAD per ack, should be optimized

            for (const Ack& ack : this->acklist) {
                // TODO: Return information back to the caller

                flusher.flush_if_full(output);

                base_segment.header.sn = ack.sn;
                base_segment.header.ts = ack.ts;

                flusher.encode(base_segment);
            }

            this->acklist.clear();
        }
    };
}