#pragma once

#include <span>
#include "types.hpp"

namespace imkcpp {
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

    private:
        UT value = 0;
    };

    namespace encoder {
        template<>
        constexpr size_t encoded_size<Fragment>() {
            return encoded_size<Fragment::UT>();
        }

        template<>
        inline void encode<Fragment>(std::span<std::byte>& buf, size_t& offset, const Fragment value) {
            encode<Fragment::UT>(buf, offset, value.get());
        }

        template<>
        inline void decode<Fragment>(const std::span<const std::byte>& buf, size_t& offset, Fragment& value) {
            Fragment::UT decodedValue = 0;
            decode<Fragment::UT>(buf, offset, decodedValue);
            value = Fragment(decodedValue);
        }
    }
}