#pragma once

#include <span>
#include <cstddef>
#include <cstring>
#include <cassert>
#include "types.hpp"

namespace imkcpp::encoder {
    inline u16 htons(const u16 hostshort) {
        return hostshort >> 8 |
               hostshort << 8;
    }

    inline u16 ntohs(const uint16_t netshort) {
        return htons(netshort);
    }

    inline u32 htonl(const u32 hostlong) {
        return hostlong >> 24 & 0xFF |
               hostlong << 8 & 0xFF0000 |
               hostlong >> 8 & 0xFF00 |
               hostlong << 24 & 0xFF000000;
    }

    inline u32 ntohl(const uint32_t netlong) {
        return htonl(netlong);
    }

    template<typename T>
    using is_allowed_type = std::disjunction<std::is_same<T, u8>, std::is_same<T, u16>, std::is_same<T, u32>>;

    template<typename T>
    T hton(T value) {
        static_assert(is_allowed_type<T>::value, "Type not allowed. Allowed types are u8, u16, u32.");

        if constexpr (std::is_same_v<T, u16>) return htons(value);
        else if constexpr (std::is_same_v<T, u32>) return htonl(value);
        else return value;
    }

    template<typename T>
    T ntoh(T value) {
        static_assert(is_allowed_type<T>::value, "Type not allowed. Allowed types are u8, u16, u32.");

        if constexpr (std::is_same_v<T, u16>) return ntohs(value);
        else if constexpr (std::is_same_v<T, u32>) return ntohl(value);
        else return value;
    }

    template<typename T>
    void encode(std::span<std::byte>& buf, size_t& offset, const T value) {
        static_assert(is_allowed_type<T>::value, "Type not allowed. Allowed types are u8, u16, u32.");
        assert(buf.size() >= offset + sizeof(T));

        T networkValue = hton(value);
        std::memcpy(buf.data() + offset, &networkValue, sizeof(T));
        offset += sizeof(T);
    }

    template<typename T>
    void decode(const std::span<const std::byte>& buf, size_t& offset, T& value) {
        static_assert(is_allowed_type<T>::value, "Type not allowed. Allowed types are u8, u16, u32.");
        assert(buf.size() >= offset + sizeof(T));

        T networkValue;
        std::memcpy(&networkValue, buf.data() + offset, sizeof(T));
        value = ntoh(networkValue);
        offset += sizeof(T);
    }
}