#pragma once

#include "types.hpp"

namespace imkcpp {
    struct Ack {
        u32 sn, ts;

        explicit Ack() : sn(0), ts(0) { }
        explicit Ack(const u32 sn, const u32 ts) : sn(sn), ts(ts) { }
    };
}