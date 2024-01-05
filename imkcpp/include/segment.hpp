#pragma once

#include "types.hpp"
#include <vector>

struct segment {
    u32 conv, cmd, frg;
    u32 wnd, ts, sn, una;
    u32 len;
    u32 resendts, rto;
    u32 fastack, xmit;
    std::vector<std::byte> data;

    explicit segment(const size_t size) : conv(0), cmd(0), frg(0),
                                          wnd(0), ts(0), sn(0), una(0),
                                          len(0),
                                          resendts(0), rto(0),
                                          fastack(0), xmit(0),
                                          data(size) {
    }
};