#pragma once

#include <cstdint>

namespace imkcpp::constants {
    // TODO: These need to be documented and explained
    constexpr uint32_t IKCP_RTO_NDL = 30;		// no delay min rto
    constexpr uint32_t IKCP_RTO_MIN = 100;		// normal min rto
    constexpr uint32_t IKCP_RTO_DEF = 200;
    constexpr uint32_t IKCP_RTO_MAX = 60000;
    constexpr uint32_t IKCP_ASK_SEND = 1;		// need to send IKCP_CMD_WASK
    constexpr uint32_t IKCP_ASK_TELL = 2;		// need to send IKCP_CMD_WINS
    constexpr uint32_t IKCP_WND_SND = 32;
    constexpr uint32_t IKCP_WND_RCV = 128;       // must >= max fragment size
    constexpr uint32_t IKCP_MTU_DEF = 1400;
    constexpr uint32_t IKCP_INTERVAL = 100;
    constexpr uint32_t IKCP_OVERHEAD = 24;
    constexpr uint32_t IKCP_DEADLINK = 20;
    constexpr uint32_t IKCP_THRESH_INIT = 2;
    constexpr uint32_t IKCP_THRESH_MIN = 2;
    constexpr uint32_t IKCP_PROBE_INIT = 7000;		// 7 secs to probe window size
    constexpr uint32_t IKCP_PROBE_LIMIT = 120000;	// up to 120 secs to probe window
    constexpr uint32_t IKCP_FASTACK_LIMIT = 5;		// max times to trigger fastack
}