#pragma once

#include <span>
#include "types.hpp"

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

    private:
        UT value = 0;
    };

    namespace encoder {
        template<>
        constexpr size_t encoded_size<PayloadLen>() {
            return encoded_size<PayloadLen::UT>();
        }

        template<>
        inline void encode<PayloadLen>(std::span<std::byte>& buf, size_t& offset, const PayloadLen value) {
            encode<PayloadLen::UT>(buf, offset, value.get());
        }

        template<>
        inline void decode<PayloadLen>(const std::span<const std::byte>& buf, size_t& offset, PayloadLen& value) {
            PayloadLen::UT decodedValue = 0;
            decode<PayloadLen::UT>(buf, offset, decodedValue);
            value = PayloadLen(decodedValue);
        }
    }
}