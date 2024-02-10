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
        // TODO: As we use milliseconds and chrono can be fed with :now(),
        // TODO: this can overflow u32. We should provide a way to use a different
        // TODO: epoch, or a different time unit.
        template<>
        constexpr size_t encoded_size<timepoint_t>() {
            return encoded_size<u32>();
        }

        template<>
        inline void encode<timepoint_t>(std::span<std::byte>& buf, size_t& offset, const timepoint_t& value) {
            const auto ms = std::chrono::duration_cast<milliseconds_t>(value.time_since_epoch()).count();
            encode<u32>(buf, offset, static_cast<u32>(ms));
        }

        template<>
        inline void decode<timepoint_t>(const std::span<const std::byte>& buf, size_t& offset, timepoint_t& value) {
            u32 decodedValue;
            decode<u32>(buf, offset, decodedValue);
            value = timepoint_t(std::chrono::milliseconds(decodedValue));
        }
    }
}