#pragma once

namespace imkcpp::commands {
    constexpr uint32_t IKCP_CMD_PUSH = 81;
    constexpr uint32_t IKCP_CMD_ACK  = 82;
    constexpr uint32_t IKCP_CMD_WASK = 83;
    constexpr uint32_t IKCP_CMD_WINS = 84;

    inline bool is_valid(const uint32_t cmd) {
        return cmd >= IKCP_CMD_PUSH && cmd <= IKCP_CMD_WINS;
    }
}