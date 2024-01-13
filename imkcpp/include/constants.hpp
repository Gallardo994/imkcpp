#pragma once

#include "types.hpp"

namespace imkcpp::constants {
    // TODO: These need to be documented and explained
    constexpr u32 IKCP_RTO_NDL = 30;		// no delay min rto
    constexpr u32 IKCP_RTO_MIN = 100;		// normal min rto
    constexpr u32 IKCP_RTO_DEF = 200;
    constexpr u32 IKCP_RTO_MAX = 60000;
    constexpr u32 IKCP_ASK_SEND = 1;		// need to send IKCP_CMD_WASK
    constexpr u32 IKCP_ASK_TELL = 2;		// need to send IKCP_CMD_WINS
    constexpr u32 IKCP_WND_SND = 32;
    constexpr u32 IKCP_WND_RCV = 128;       // must >= max fragment size
    constexpr u32 IKCP_MTU_DEF = 1400;
    constexpr u32 IKCP_INTERVAL = 100;
    constexpr u32 IKCP_OVERHEAD = 24;
    constexpr u32 IKCP_DEADLINK = 20;
    constexpr u32 IKCP_THRESH_INIT = 2;
    constexpr u32 IKCP_THRESH_MIN = 2;
    constexpr u32 IKCP_PROBE_INIT = 7000;		// 7 secs to probe window size
    constexpr u32 IKCP_PROBE_LIMIT = 120000;	// up to 120 secs to probe window
    constexpr u32 IKCP_FASTACK_LIMIT = 5;		// max times to trigger fastack
}