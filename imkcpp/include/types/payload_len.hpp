#pragma once

#include <span>
#include "types.hpp"
#include "serializer.hpp"

namespace imkcpp {
    /// Payload length.
    /// Determines the length of the payload.
    class PayloadLen final {
    public:
        using UT = u32;

        constexpr explicit PayloadLen(const UT value) noexcept : value(value) {}

        [[nodiscard]] constexpr UT get() const noexcept {
            return this->value;
        }

        constexpr bool operator>(const size_t& other) const noexcept {
            return this->value > other;
        }

        constexpr bool operator<(const size_t& other) const noexcept {
            return this->value < other;
        }

        void serialize(const std::span<std::byte> buf, size_t& offset) const {
            serializer::serialize<UT>(this->value, buf, offset);
        }

        void deserialize(const std::span<const std::byte> buf, size_t& offset) {
            serializer::deserialize<UT>(this->value, buf, offset);
        }

        [[nodiscard]] constexpr static size_t fixed_size() {
            return serializer::fixed_size<UT>();
        }

    private:
        UT value = 0;
    };
}