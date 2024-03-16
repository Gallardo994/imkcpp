#pragma once

#include <span>
#include "types.hpp"
#include "serializer.hpp"

namespace imkcpp {
    /// Conversation ID.
    /// Used for splitting channels over the same socket. UT can be changed to u16 or u8 to save space if needed.
    class Conv final {
    public:
        using UT = u32;

        constexpr explicit Conv(const UT value) noexcept : value(value) {}

        [[nodiscard]] constexpr UT get() const noexcept {
            return this->value;
        }

        constexpr bool operator==(const Conv& other) const noexcept {
            return this->value == other.value;
        }

        constexpr bool operator!=(const Conv& other) const noexcept {
            return !(*this == other);
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

        static_assert(serializer::Serializable<Conv>);
        static_assert(serializer::FixedSize<Conv>);
    };
}