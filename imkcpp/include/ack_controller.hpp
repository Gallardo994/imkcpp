#pragma once

#include "types.hpp"
#include "ack.hpp"
#include "flusher.hpp"

namespace imkcpp {
    class AckController {
        Flusher& flusher;
        SharedCtx& shared_ctx;

        std::vector<Ack> acklist{};

    public:
        explicit AckController(Flusher& flusher, SharedCtx& shared_ctx) : flusher(flusher), shared_ctx(shared_ctx) {}

        [[nodiscard]] bool should_acknowledge(const u32 sn) const {
            if (time_delta(sn, shared_ctx.snd_una) < 0 || time_delta(sn, shared_ctx.snd_nxt) >= 0) {
                return false;
            }

            return true;
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