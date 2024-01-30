#pragma once

#include <span>
#include "types.hpp"

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

    private:
        UT value = 0;
    };

    namespace encoder {
        template<>
        constexpr size_t encoded_size<Conv>() {
            return encoded_size<Conv::UT>();
        }

        template<>
        inline void encode<Conv>(std::span<std::byte>& buf, size_t& offset, const Conv value) {
            encode<Conv::UT>(buf, offset, value.get());
        }

        template<>
        inline void decode<Conv>(const std::span<const std::byte>& buf, size_t& offset, Conv& value) {
            Conv::UT decodedValue = 0;
            decode<Conv::UT>(buf, offset, decodedValue);
            value = Conv(decodedValue);
        }
    }
}