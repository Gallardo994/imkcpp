#pragma once

#include "types.hpp"

namespace imkcpp {
    class SegmentTracker final {
        /// Sequence number of the first unacknowledged segment.
        u32 snd_una = 0;

        /// Sequence number of the next segment to be sent.
        u32 snd_nxt = 0;

    public:
        [[nodiscard]] u32 get_snd_una() const {
            return this->snd_una;
        }

        void set_snd_una(const u32 snd_una) {
            this->snd_una = snd_una;
        }

        void reset_snd_una() {
            this->snd_una = this->snd_nxt;
        }

        [[nodiscard]] u32 get_snd_nxt() const {
            return this->snd_nxt;
        }

        [[nodiscard]] u32 get_and_increment_snd_nxt() {
            return this->snd_nxt++;
        }

        [[nodiscard]] u32 get_packets_in_flight_count() const {
            assert(this->snd_nxt >= this->snd_una);

            return this->snd_nxt - this->snd_una;
        }
    };
}
