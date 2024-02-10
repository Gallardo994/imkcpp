#pragma once

#include "types.hpp"
#include "clock.hpp"

namespace imkcpp {
    using namespace std::chrono_literals;

    /// Shared context between the sender and the receiver.
    class SharedCtx final {
        /// Current state of the connection.
        State state = State::Alive;

        /// Conversation ID.
        Conv conv{0};

        /// Interval.
        duration_t interval = milliseconds_t(constants::IKCP_INTERVAL);

    public:
        /// Gets the current state of the connection.
        [[nodiscard]] State get_state() const {
            return this->state;
        }

        /// Sets the current state of the connection.
        void set_state(const State state) {
            this->state = state;
        }

        [[nodiscard]] Conv get_conv() const {
            return this->conv;
        }

        void set_conv(const Conv conv) {
            this->conv = conv;
        }

        [[nodiscard]] duration_t get_interval() const {
            return this->interval;
        }

        void set_interval(const duration_t interval) {
            this->interval = interval;
        }
    };
}