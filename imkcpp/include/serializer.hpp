#pragma once

#include <span>
#include <cstddef>
#include <concepts>
#include <type_traits>
#include "types.hpp"
#include "endian.hpp"

namespace imkcpp::serializer {

    // Serialization and deserialization for custom types

    template<typename T>
    concept Serializable = requires(T& t, std::span<std::byte> writeBuf, const std::span<const std::byte>& readBuf, size_t& offset) {
        { t.serialize(writeBuf, offset) } -> std::same_as<void>;
        { t.deserialize(readBuf, offset) } -> std::same_as<void>;
    };

    template<Serializable T>
    void serialize(const T& value, std::span<std::byte> buf, size_t& offset) {
        value.serialize(buf, offset);
    }

    template<Serializable T>
    void deserialize(T& value, const std::span<const std::byte> buf, size_t& offset) {
        value.deserialize(buf, offset);
    }

    // Fixed size for custom types

    template<typename T>
    concept FixedSize = requires {
        { T::fixed_size() } -> std::same_as<size_t>;
    };

    template<FixedSize T>
    constexpr size_t fixed_size() {
        return T::fixed_size();
    };

    // Serialization and deserialization for non-custom types

    template<typename T>
    struct TraitSerializable;

    template<>
    struct TraitSerializable<u8> {
        static void serialize(const u8 value, std::span<std::byte> buf, size_t& offset) {
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
    struct TraitSerializable<u16> {
        static void serialize(const u16 value, std::span<std::byte> buf, size_t& offset) {
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
    struct TraitSerializable<u32> {
        static void serialize(const u32 value, std::span<std::byte> buf, size_t& offset) {
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

    template<typename T>
    concept TraitSerializableConcept = requires(T& t, std::span<std::byte>& writeBuf, const std::span<const std::byte>& readBuf, size_t& offset) {
        { TraitSerializable<T>::serialize(t, writeBuf, offset) } -> std::same_as<void>;
        { TraitSerializable<T>::deserialize(t, readBuf, offset) } -> std::same_as<void>;
    };

    template<TraitSerializableConcept T>
    void serialize(const T value, std::span<std::byte> buf, size_t& offset) {
        TraitSerializable<T>::serialize(value, buf, offset);
    }

    template<TraitSerializableConcept T>
    void deserialize(T& value, const std::span<const std::byte> buf, size_t& offset) {
        TraitSerializable<T>::deserialize(value, buf, offset);
    }

    // Fixed size for non-custom types

    template<typename T>
    struct TraitFixedSize;

    template<>
    struct TraitFixedSize<u8> {
        static constexpr size_t fixed_size() { return sizeof(u8); }
    };

    template<>
    struct TraitFixedSize<u16> {
        static constexpr size_t fixed_size() { return sizeof(u16); }
    };

    template<>
    struct TraitFixedSize<u32> {
        static constexpr size_t fixed_size() { return sizeof(u32); }
    };

    template<typename T>
    concept TraitFixedSizeConcept = requires {
        { TraitFixedSize<T>::fixed_size() } -> std::same_as<size_t>;
    };

    template<TraitFixedSizeConcept T>
    constexpr size_t fixed_size() {
        return TraitFixedSize<T>::fixed_size();
    }
}
