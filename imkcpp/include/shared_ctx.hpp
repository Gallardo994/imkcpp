#pragma once

#include "types.hpp"

namespace imkcpp {
    class SharedCtx {
        State state = State::Alive;

    public:
        // TODO: Make private and add meaningful getters/setters
        u32 conv = 0;
        size_t mtu = 0;
        size_t mss = 0;

        u32 snd_una = 0;
        u32 snd_nxt = 0;
        u32 rcv_nxt = 0;

        u32 interval = constants::IKCP_INTERVAL;

        [[nodiscard]] State get_state() const {
            return this->state;
        }

        void set_state(const State state) {
            this->state = state;
        }
    };
}