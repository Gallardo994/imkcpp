#pragma once

#include <span>
#include <cstddef>
#include <cassert>
#include "types.hpp"

namespace imkcpp::encoder {
    inline void encode8u(std::span<std::byte>& buf, size_t& offset, u8 value) {
        assert(buf.size() >= offset + sizeof(u8));
        std::memcpy(buf.data() + offset, &value, sizeof(u8));
        offset += sizeof(u8);
    }

    inline void decode8u(const std::span<const std::byte>& buf, size_t& offset, u8& value) {
        assert(buf.size() >= offset + sizeof(u8));
        std::memcpy(&value, buf.data() + offset, sizeof(u8));
        offset += sizeof(u8);
    }

    inline void encode16u(std::span<std::byte>& buf, size_t& offset, u16 value) {
        assert(buf.size() >= offset + sizeof(u16));
        std::memcpy(buf.data() + offset, &value, sizeof(u16));
        offset += sizeof(u16);
    }

    inline void decode16u(const std::span<const std::byte>& buf, size_t& offset, u16& value) {
        assert(buf.size() >= offset + sizeof(u16));
        std::memcpy(&value, buf.data() + offset, sizeof(u16));
        offset += sizeof(u16);
    }

    inline void encode32u(std::span<std::byte>& buf, size_t& offset, u32 value) {
        assert(buf.size() >= offset + sizeof(u32));
        std::memcpy(buf.data() + offset, &value, sizeof(u32));
        offset += sizeof(u32);
    }

    inline void decode32u(const std::span<const std::byte>& buf, size_t& offset, u32& value) {
        assert(buf.size() >= offset + sizeof(u32));
        std::memcpy(&value, buf.data() + offset, sizeof(u32));
        offset += sizeof(u32);
    }
}