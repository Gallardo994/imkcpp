#pragma once

#include <span>
#include <cstddef>
#include <cstring>
#include <cassert>
#include "types.hpp"

#ifndef IWORDS_BIG_ENDIAN
    #ifdef _BIG_ENDIAN_
        #if _BIG_ENDIAN_
            #define IWORDS_BIG_ENDIAN 1
        #endif
    #endif

    #ifndef IWORDS_BIG_ENDIAN
        #if defined(__hppa__) || \
        defined(__m68k__) || defined(mc68000) || defined(_M_M68K) || \
        (defined(__MIPS__) && defined(__MIPSEB__)) || \
        defined(__ppc__) || defined(__POWERPC__) || defined(_M_PPC) || \
        defined(__sparc__) || defined(__powerpc__) || \
        defined(__mc68000__) || defined(__s390x__) || defined(__s390__)
            #define IWORDS_BIG_ENDIAN 1
        #endif
    #endif

    #ifndef IWORDS_BIG_ENDIAN
        #define IWORDS_BIG_ENDIAN  0
    #endif
#endif

#ifndef IWORDS_MUST_ALIGN
    #if defined(__i386__) || defined(__i386) || defined(_i386_)
        #define IWORDS_MUST_ALIGN 0
    #elif defined(_M_IX86) || defined(_X86_) || defined(__x86_64__)
        #define IWORDS_MUST_ALIGN 0
    #elif defined(__amd64) || defined(__amd64__)
        #define IWORDS_MUST_ALIGN 0
    #else
        #define IWORDS_MUST_ALIGN 1
    #endif
#endif

namespace imkcpp::encoder {
    inline uint16_t htons(const uint16_t hostshort) {
#if IWORDS_BIG_ENDIAN || IWORDS_MUST_ALIGN
        return hostshort;
#else
        return hostshort >> 8 |
               hostshort << 8;
#endif
    }

    inline uint16_t ntohs(const uint16_t netshort) {
        return htons(netshort);
    }

    inline uint32_t htonl(const uint32_t hostlong) {
#if IWORDS_BIG_ENDIAN || IWORDS_MUST_ALIGN
        return hostlong;
#else
        return hostlong >> 24 & 0xFF |
               hostlong << 8 & 0xFF0000 |
               hostlong >> 8 & 0xFF00 |
               hostlong << 24 & 0xFF000000;
#endif
    }

    inline uint32_t ntohl(const uint32_t netlong) {
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