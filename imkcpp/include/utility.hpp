#pragma once

#include "types.hpp"

namespace imkcpp {
    /// Returns i32 value of the difference between two u32 values, aka a - b.
    static i32 time_delta(const u32 later, const u32 earlier) {
        return static_cast<i32>(later - earlier);
    }

    /// Calculates Maximum Segment Size from Maximum Transmission Unit.
    template <size_t MTU>
    constexpr size_t MTU_TO_MSS() {
        static_assert(MTU >= constants::IKCP_OVERHEAD + 1);
        return MTU - constants::IKCP_OVERHEAD;
    }
}