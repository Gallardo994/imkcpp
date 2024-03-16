#pragma once

#include <cstdint>
#include <functional>
#include <span>
#include "serializer.hpp"
#include "endian.hpp"

namespace imkcpp {
    using i8 = int8_t;
    using u8 = uint8_t;

    using i16 = int16_t;
    using u16 = uint16_t;

    using i32 = int32_t;
    using u32 = uint32_t;

    using i64 = int64_t;
    using u64 = uint64_t;

    using output_callback_t = std::function<void(std::span<const std::byte>)>;

    template<>
    struct serializer::TraitSerializable<u8> {
        static void serialize(const u8 value, const std::span<std::byte> buf, size_t& offset) {
            assert(buf.size() >= offset + sizeof(u8));
            std::memcpy(buf.data() + offset, &value, sizeof(u8));
            offset += sizeof(u8);
        }

        static void deserialize(u8& value, const std::span<const std::byte> buf, size_t& offset) {
            assert(buf.size() >= offset + sizeof(u8));
            std::memcpy(&value, buf.data() + offset, sizeof(u8));
            offset += sizeof(u8);
        }
    };

    template<>
    struct serializer::TraitSerializable<u16> {
        static void serialize(const u16 value, const std::span<std::byte> buf, size_t& offset) {
            assert(buf.size() >= offset + sizeof(u16));
            const u16 networkValue = endian::htons(value);
            std::memcpy(buf.data() + offset, &networkValue, sizeof(u16));
            offset += sizeof(u16);
        }

        static void deserialize(u16& value, const std::span<const std::byte> buf, size_t& offset) {
            assert(buf.size() >= offset + sizeof(u16));
            std::memcpy(&value, buf.data() + offset, sizeof(u16));
            value = endian::ntohs(value);
            offset += sizeof(u16);
        }
    };

    template<>
    struct serializer::TraitSerializable<u32> {
        static void serialize(const u32 value, const std::span<std::byte> buf, size_t& offset) {
            assert(buf.size() >= offset + sizeof(u32));
            const u32 networkValue = endian::htonl(value);
            std::memcpy(buf.data() + offset, &networkValue, sizeof(u32));
            offset += sizeof(u32);
        }

        static void deserialize(u32& value, const std::span<const std::byte> buf, size_t& offset) {
            assert(buf.size() >= offset + sizeof(u32));
            std::memcpy(&value, buf.data() + offset, sizeof(u32));
            value = endian::ntohl(value);
            offset += sizeof(u32);
        }
    };

    template<>
    struct serializer::TraitFixedSize<u8> {
        static constexpr size_t fixed_size() {
            return sizeof(u8);
        }
    };

    template<>
    struct serializer::TraitFixedSize<u16> {
        static constexpr size_t fixed_size() {
            return sizeof(u16);
        }
    };

    template<>
    struct serializer::TraitFixedSize<u32> {
        static constexpr size_t fixed_size() {
            return sizeof(u32);
        }
    };
}