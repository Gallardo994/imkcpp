#pragma once

#include "types.hpp"

namespace imkcpp {
    class SharedCtx {
        State state = State::Alive; // Current state of the connection.

        u32 conv = 0; // Conversation ID

        // TODO: These have to be compile-time values
        size_t mtu = 0; // Maximum Transmission Unit
        size_t mss = 0; // Maximum Segment Size (mtu - header size)

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

        [[nodiscard]] size_t get_mtu() const {
            return this->mtu;
        }

        void set_mtu(const size_t mtu) {
            this->mtu = mtu;
            this->mss = mtu - constants::IKCP_OVERHEAD;
        }

        [[nodiscard]] size_t get_mss() const {
            return this->mss;
        }

        [[nodiscard]] u32 get_interval() const {
            return this->interval;
        }

        void set_interval(const u32 interval) {
            this->interval = interval;
        }
    };
}