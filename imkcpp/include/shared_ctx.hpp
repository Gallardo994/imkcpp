#pragma once

#include "types.hpp"

namespace imkcpp {
    class SharedCtx {
        State state = State::Alive;

    public:
        // TODO: Make private and add meaningful getters/setters
        u32 conv = 0; // Conversation ID
        size_t mtu = 0; // Maximum Transmission Unit
        size_t mss = 0; // Maximum Segment Size (mtu - header size)

        u32 interval = constants::IKCP_INTERVAL; // Interval.

        // Gets the current state of the connection.
        [[nodiscard]] State get_state() const {
            return this->state;
        }

        // Sets the current state of the connection.
        void set_state(const State state) {
            this->state = state;
        }
    };
}