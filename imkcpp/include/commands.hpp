#pragma once

#include "types.hpp"

namespace imkcpp::commands {
    // TODO: Rework this to be a proper enum class
    constexpr u8 IKCP_CMD_PUSH = 81;
    constexpr u8 IKCP_CMD_ACK  = 82;
    constexpr u8 IKCP_CMD_WASK = 83;
    constexpr u8 IKCP_CMD_WINS = 84;

    inline bool is_valid(const u8 cmd) {
        return cmd >= IKCP_CMD_PUSH && cmd <= IKCP_CMD_WINS;
    }
}