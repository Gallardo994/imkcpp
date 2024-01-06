#pragma once

#include "types.hpp"

namespace encoder {
    // OLD
    inline std::byte* encode8u(std::byte* p, u8 c) {
        *reinterpret_cast<uint8_t*>(p++) = c;
        return p;
    }

    inline const std::byte* decode8u(const std::byte* p, u8& c) {
        c = *reinterpret_cast<const uint8_t*>(p++);
        return p;
    }

    inline std::byte* encode16u(std::byte* p, u16 w) {
        std::memcpy(p, &w, sizeof(u16));
        p += sizeof(u16);
        return p;
    }

    inline const std::byte* decode16u(const std::byte* p, u16& w) {
        std::memcpy(&w, p, sizeof(u16));
        p += sizeof(u16);
        return p;
    }

    inline std::byte* encode32u(std::byte* p, u32 l) {
        std::memcpy(p, &l, sizeof(u32));
        p += sizeof(u32);
        return p;
    }

    inline const std::byte* decode32u(const std::byte* p, u32& l) {
        std::memcpy(&l, p, sizeof(u32));
        p += sizeof(u32);
        return p;
    }

    // NEW
    inline void encode8u(std::vector<std::byte>& buf, u16 c) {
        buf.push_back(static_cast<std::byte>(c));
    }

    inline std::vector<std::byte>::const_iterator decode8u(std::vector<std::byte>::const_iterator iter, u16& c) {
        c = static_cast<u16>(*iter);
        return ++iter;
    }

    inline void encode16u(std::vector<std::byte>& buf, u16 w) {
        std::byte temp[sizeof(u16)];
        std::memcpy(temp, &w, sizeof(u16));
        buf.insert(buf.end(), std::begin(temp), std::end(temp));
    }

    inline std::vector<std::byte>::const_iterator decode16u(std::vector<std::byte>::const_iterator iter, u16& w) {
        std::memcpy(&w, &(*iter), sizeof(u16));
        return std::next(iter, sizeof(u16));
    }

    inline void encode32u(std::vector<std::byte>& buf, u32 l) {
        std::byte temp[sizeof(u32)];
        std::memcpy(temp, &l, sizeof(u32));
        buf.insert(buf.end(), std::begin(temp), std::end(temp));
    }

    inline std::vector<std::byte>::const_iterator decode32u(std::vector<std::byte>::const_iterator iter, u32& l) {
        std::memcpy(&l, &(*iter), sizeof(u32));
        return std::next(iter, sizeof(u32));
    }

    inline void encode_raw(std::vector<std::byte>& buf, const std::vector<std::byte>& data) {
        buf.insert(buf.end(), std::begin(data), std::end(data));
    }
}