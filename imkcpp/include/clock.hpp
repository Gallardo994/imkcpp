#pragma once

#include <chrono>
#include <span>
#include "types.hpp"
#include "encoder.hpp"

namespace imkcpp {
    using namespace std::chrono_literals;

    using timepoint_t = std::chrono::steady_clock::time_point;
    using duration_t = std::chrono::steady_clock::duration;
    using milliseconds_t = std::chrono::milliseconds;

    namespace encoder {
        // Base time of 1 January 2024 00:00:00 UTC, so we can encode timepoints as u32
        constexpr timepoint_t base_time = timepoint_t(std::chrono::milliseconds(1704067200000));

        template<>
        constexpr size_t encoded_size<timepoint_t>() {
            return encoded_size<u32>();
        }

        template<>
        inline void encode<timepoint_t>(std::span<std::byte>& buf, size_t& offset, const timepoint_t& value) {
            const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(value - base_time).count();
            encode<u32>(buf, offset, static_cast<u32>(ms));
        }

        template<>
        inline void decode<timepoint_t>(const std::span<const std::byte>& buf, size_t& offset, timepoint_t& value) {
            u32 decodedValue;
            decode<u32>(buf, offset, decodedValue);
            value = base_time + std::chrono::milliseconds(decodedValue);
        }
    }
}