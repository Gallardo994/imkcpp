#pragma once

#include "types.hpp"

namespace imkcpp::constants {
    // TODO: These need to be documented and explained
    constexpr u32 IKCP_RTO_NDL = 30;		// no delay min rto
    constexpr u32 IKCP_RTO_MIN = 100;		// normal min rto
    constexpr u32 IKCP_RTO_DEF = 200;
    constexpr u32 IKCP_RTO_MAX = 60000;
    constexpr u32 IKCP_WND_SND = 32;
    constexpr u32 IKCP_WND_RCV = 128;       // must >= max fragment size
    constexpr u32 IKCP_MTU_DEF = 1400;
    constexpr u32 IKCP_INTERVAL = 100;
    constexpr u32 IKCP_DEADLINK = 20;
    constexpr u32 IKCP_THRESH_INIT = 2;
    constexpr u32 IKCP_THRESH_MIN = 2;
    constexpr u32 IKCP_FASTACK_LIMIT = 5;		// max times to trigger fastack
}