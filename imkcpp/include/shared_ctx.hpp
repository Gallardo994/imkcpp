#pragma once

#include "types.hpp"

namespace imkcpp {
    class SharedCtx {
    public:
        // TODO: Make private and add meaningful getters/setters
        u32 snd_una = 0;
        u32 snd_nxt = 0;
        u32 rcv_nxt = 0;
    };
}