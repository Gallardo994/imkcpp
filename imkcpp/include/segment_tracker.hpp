#pragma once

#include "types.hpp"

namespace imkcpp {
    // TODO: Add logical methods like "can_receive" and "is_in_flight", etc
    class SegmentTracker final {
        u32 snd_una = 0; // Sequence number of the first unacknowledged segment.
        u32 snd_nxt = 0; // Sequence number of the next segment to be sent.
        u32 rcv_nxt = 0; // Sequence number of the next segment to be received.

    public:
        // snd_una

        [[nodiscard]] u32 get_snd_una() const {
            return this->snd_una;
        }

        void set_snd_una(const u32 snd_una) {
            this->snd_una = snd_una;
        }

        void reset_snd_una() {
            this->snd_una = this->snd_nxt;
        }

        // snd_nxt

        [[nodiscard]] u32 get_snd_nxt() const {
            return this->snd_nxt;
        }

        [[nodiscard]] u32 get_and_increment_snd_nxt() {
            return this->snd_nxt++;
        }

        // rcv_nxt

        [[nodiscard]] u32 get_rcv_nxt() const {
            return this->rcv_nxt;
        }

        void increment_rcv_nxt() {
            this->rcv_nxt++;
        }

        // Other

        [[nodiscard]] u32 get_packets_in_flight_count() const {
            assert(this->snd_nxt >= this->snd_una);

            return this->snd_nxt - this->snd_una;
        }

        [[nodiscard]] bool should_receive(const u32 sn) const {
            return sn >= this->rcv_nxt;
        }
    };
}
