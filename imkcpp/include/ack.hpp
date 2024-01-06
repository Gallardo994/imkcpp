#pragma once

#include "types.hpp"

struct Ack {
    u32 sn, ts;

    explicit Ack(const u32 sn, const u32 ts) : sn(sn), ts(ts) { }
};