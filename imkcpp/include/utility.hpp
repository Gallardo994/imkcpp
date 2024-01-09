#pragma once

#include "types.hpp"

namespace imkcpp {
    static i32 time_delta(const u32 later, const u32 earlier) {
        return static_cast<i32>(later - earlier);
    }
}