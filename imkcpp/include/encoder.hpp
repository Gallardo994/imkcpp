#pragma once

#include "types.hpp"

namespace encoder {
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
}