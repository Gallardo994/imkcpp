#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <span>

namespace imkcpp {
    class ImKcpp;

    using i8 = int8_t;
    using u8 = uint8_t;

    using i16 = int16_t;
    using u16 = uint16_t;

    using i32 = int32_t;
    using u32 = uint32_t;

    using i64 = int64_t;
    using u64 = uint64_t;

    using output_callback_t = std::function<void(std::span<const std::byte>, const ImKcpp&, std::optional<void*>)>;
}