#pragma once

#include <span>
#include "types.hpp"
#include "serializer.hpp"

namespace imkcpp {
    /// Command ID.
    /// Determines the type of the segment and the action to be taken.
    class Cmd final {
    public:
        using UT = u8;

        constexpr explicit Cmd(const UT value) noexcept : value(value) {}

        [[nodiscard]] constexpr UT get() const noexcept {
            return this->value;
        }

        constexpr bool operator==(const Cmd& other) const noexcept {
            return this->value == other.value;
        }

        constexpr bool operator!=(const Cmd& other) const noexcept {
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

        static_assert(serializer::Serializable<Cmd>);
        static_assert(serializer::FixedSize<Cmd>);
    };
}