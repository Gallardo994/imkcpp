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
    void encode(std::span<std::byte>& buf, size_t& offset, const T& value);

    template<>
    inline void encode<u8>(std::span<std::byte>& buf, size_t& offset, const u8& value) {
        assert(buf.size() >= offset + sizeof(u8));
        std::memcpy(buf.data() + offset, &value, sizeof(u8));
        offset += sizeof(u8);
    }

    template<>
    inline void encode<u16>(std::span<std::byte>& buf, size_t& offset, const u16& value) {
        assert(buf.size() >= offset + sizeof(u16));
        const u16 networkValue = htons(value);
        std::memcpy(buf.data() + offset, &networkValue, sizeof(u16));
        offset += sizeof(u16);
    }

    template<>
    inline void encode<u32>(std::span<std::byte>& buf, size_t& offset, const u32& value) {
        assert(buf.size() >= offset + sizeof(u32));
        const u32 networkValue = htonl(value);
        std::memcpy(buf.data() + offset, &networkValue, sizeof(u32));
        offset += sizeof(u32);
    }

    template<typename T>
    void decode(const std::span<const std::byte>& buf, size_t& offset, T& value);

    template<>
    inline void decode<u8>(const std::span<const std::byte>& buf, size_t& offset, u8& value) {
        assert(buf.size() >= offset + sizeof(u8));
        std::memcpy(&value, buf.data() + offset, sizeof(u8));
        offset += sizeof(u8);
    }

    template<>
    inline void decode<u16>(const std::span<const std::byte>& buf, size_t& offset, u16& value) {
        assert(buf.size() >= offset + sizeof(u16));
        std::memcpy(&value, buf.data() + offset, sizeof(u16));
        value = ntohs(value);
        offset += sizeof(u16);
    }

    template<>
    inline void decode<u32>(const std::span<const std::byte>& buf, size_t& offset, u32& value) {
        assert(buf.size() >= offset + sizeof(u32));
        std::memcpy(&value, buf.data() + offset, sizeof(u32));
        value = ntohl(value);
        offset += sizeof(u32);
    }

    template<typename T>
    size_t encoded_size();

    template<>
    constexpr size_t encoded_size<u8>() {
        return sizeof(u8);
    }

    template<>
    constexpr size_t encoded_size<u16>() {
        return sizeof(u16);
    }

    template<>
    constexpr size_t encoded_size<u32>() {
        return sizeof(u32);
    }
}