#pragma once

#include <span>
#include "types.hpp"

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

    private:
        UT value = 0;
    };

    namespace encoder {
        template<>
        constexpr size_t encoded_size<Cmd>() {
            return encoded_size<Cmd::UT>();
        }

        template<>
        inline void encode<Cmd>(std::span<std::byte>& buf, size_t& offset, const Cmd value) {
            encode<Cmd::UT>(buf, offset, value.get());
        }

        template<>
        inline void decode<Cmd>(const std::span<const std::byte>& buf, size_t& offset, Cmd& value) {
            Cmd::UT decodedValue = 0;
            decode<Cmd::UT>(buf, offset, decodedValue);
            value = Cmd(decodedValue);
        }
    }
}