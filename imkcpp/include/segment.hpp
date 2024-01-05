#pragma once

#include <cstdint>
#include <vector>

struct segment {
    uint32_t conv, cmd, frg;
    uint32_t wnd, ts, sn, una;
    uint32_t len;
    uint32_t resendts, rto;
    uint32_t fastack, xmit;
    std::vector<std::byte> data;
};