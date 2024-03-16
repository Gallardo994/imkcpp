#pragma once

#include <span>
#include <cstddef>
#include <concepts>
#include <type_traits>

namespace imkcpp::serializer {

    // Serialization and deserialization for custom types

    template<typename T>
    concept Serializable = requires(T& t, const std::span<std::byte> writeBuf, const std::span<const std::byte> readBuf, size_t& offset) {
        { t.serialize(writeBuf, offset) } -> std::same_as<void>;
        { t.deserialize(readBuf, offset) } -> std::same_as<void>;
    };

    template<Serializable T>
    void serialize(const T& value, const std::span<std::byte> buf, size_t& offset) {
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

    // Dynamic size for custom types

    template<typename T>
    concept DynamicSize = requires(T& t) {
        { t.dynamic_size() } -> std::same_as<size_t>;
    };

    template<DynamicSize T>
    constexpr size_t dynamic_size(const T& value) {
        return value.dynamic_size();
    }

    // Serialization and deserialization for non-custom types

    template<typename T>
    struct TraitSerializable;

    template<typename T>
    concept TraitSerializableConcept = requires(T& t, const std::span<std::byte> writeBuf, const std::span<const std::byte> readBuf, size_t& offset) {
        { TraitSerializable<T>::serialize(t, writeBuf, offset) } -> std::same_as<void>;
        { TraitSerializable<T>::deserialize(t, readBuf, offset) } -> std::same_as<void>;
    };

    template<TraitSerializableConcept T>
    void serialize(const T value, const std::span<std::byte> buf, size_t& offset) {
        TraitSerializable<T>::serialize(value, buf, offset);
    }

    template<TraitSerializableConcept T>
    void deserialize(T& value, const std::span<const std::byte> buf, size_t& offset) {
        TraitSerializable<T>::deserialize(value, buf, offset);
    }

    // Fixed size for non-custom types

    template<typename T>
    struct TraitFixedSize;

    template<typename T>
    concept TraitFixedSizeConcept = requires {
        { TraitFixedSize<T>::fixed_size() } -> std::same_as<size_t>;
    };

    template<TraitFixedSizeConcept T>
    constexpr size_t fixed_size() {
        return TraitFixedSize<T>::fixed_size();
    }
}
