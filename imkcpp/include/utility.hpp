#pragma once

#include "types.hpp"
#include "serializer.hpp"

namespace imkcpp {
    /// Returns i32 value of the difference between two u32 values, aka a - b.
    static i32 time_delta(const u32 later, const u32 earlier) {
        return static_cast<i32>(later - earlier);
    }

    /// Calculates Maximum Segment Size from Maximum Transmission Unit.
    template <size_t MTU>
    constexpr size_t MTU_TO_MSS() {
        static_assert(MTU >= serializer::fixed_size<SegmentHeader>() + 1);
        return MTU - serializer::fixed_size<SegmentHeader>();
    }
}