#pragma once

#include "types.hpp"

namespace imkcpp {
    class SharedCtx final {
        State state = State::Alive; // Current state of the connection.

        u32 conv = 0; // Conversation ID.
        u32 interval = constants::IKCP_INTERVAL; // Interval.

    public:
        // Gets the current state of the connection.
        [[nodiscard]] State get_state() const {
            return this->state;
        }

        // Sets the current state of the connection.
        void set_state(const State state) {
            this->state = state;
        }

        [[nodiscard]] u32 get_conv() const {
            return this->conv;
        }

        void set_conv(const u32 conv) {
            this->conv = conv;
        }

        [[nodiscard]] u32 get_interval() const {
            return this->interval;
        }

        void set_interval(const u32 interval) {
            this->interval = interval;
        }
    };
}