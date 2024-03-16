#pragma once

#include <span>
#include "types.hpp"
#include "serializer.hpp"

namespace imkcpp {
    /// Fragment number.
    /// Determines the order of the fragments.
    class Fragment final {
    public:
        using UT = u8;

        constexpr explicit Fragment(const UT value) noexcept : value(value) {}

        [[nodiscard]] constexpr UT get() const noexcept {
            return this->value;
        }

        constexpr bool operator==(const Fragment& other) const noexcept {
            return this->value == other.value;
        }

        constexpr bool operator!=(const Fragment& other) const noexcept {
            return !(*this == other);
        }

        constexpr bool operator<(const Fragment& other) const noexcept {
            return this->value < other.value;
        }

        constexpr bool operator>(const Fragment& other) const noexcept {
            return this->value > other.value;
        }

        constexpr bool operator==(const UT& other) const noexcept {
            return this->value == other;
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

        static_assert(serializer::Serializable<Fragment>);
        static_assert(serializer::FixedSize<Fragment>);
    };
}