#pragma once

#include "types.hpp"

namespace imkcpp {
    /// Shared context between the sender and the receiver.
    class SharedCtx final {
        /// Current state of the connection.
        State state = State::Alive;

        /// Conversation ID.
        u32 conv = 0;

        /// Interval.
        u32 interval = constants::IKCP_INTERVAL;

    public:
        /// Gets the current state of the connection.
        [[nodiscard]] State get_state() const {
            return this->state;
        }

        /// Sets the current state of the connection.
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