#pragma once

#include "types.hpp"
#include "types/conv.hpp"
#include "types/cmd.hpp"
#include "types/fragment.hpp"
#include "serializer.hpp"

#include "types/payload_len.hpp"
#include <vector>

namespace imkcpp {
    // TODO: Too many public members, make them private. Also, make the Segment class a friend of the SegmentHeader class.
    // TODO: The logic is heavily reliant on the fact that everything is public. Rework this.

    /// SegmentHeader is used to store the header of the segment.
    struct SegmentHeader final {
        /// Conversation ID.
        Conv conv{0};

        /// Command type.
        Cmd cmd{0};

        /// Fragment. Indicates how many next SNs are following this one in the same packet.
        Fragment frg{0};

        /// Window size (available space in the receive buffer).
        u16 wnd = 0;

        /// Timestamp.
        u32 ts = 0;

        /// Sequence number.
        u32 sn = 0;

        /// Unacknowledged sequence number.
        u32 una = 0;

        /// Length of the payload.
        PayloadLen len{0};

        constexpr static size_t fixed_size() {
            return serializer::fixed_size<decltype(conv)>() +
                   serializer::fixed_size<decltype(cmd)>() +
                   serializer::fixed_size<decltype(frg)>() +
                   serializer::fixed_size<decltype(wnd)>() +
                   serializer::fixed_size<decltype(ts)>() +
                   serializer::fixed_size<decltype(sn)>() +
                   serializer::fixed_size<decltype(una)>() +
                   serializer::fixed_size<decltype(len)>();
        }

        void serialize(const std::span<std::byte> buf, size_t& offset) const {
            assert(buf.size() >= serializer::fixed_size<SegmentHeader>());

            serializer::serialize<Conv>(this->conv, buf, offset);
            serializer::serialize<Cmd>(this->cmd, buf, offset);
            serializer::serialize<Fragment>(this->frg, buf, offset);
            serializer::serialize<u16>(this->wnd, buf, offset);
            serializer::serialize<u32>(this->ts, buf, offset);
            serializer::serialize<u32>(this->sn, buf, offset);
            serializer::serialize<u32>(this->una, buf, offset);
            serializer::serialize<PayloadLen>(this->len, buf, offset);
        }

        void deserialize(const std::span<const std::byte> buf, size_t& offset) {
            assert(buf.size() >= serializer::fixed_size<SegmentHeader>());

            serializer::deserialize<Conv>(this->conv, buf, offset);
            serializer::deserialize<Cmd>(this->cmd, buf, offset);
            serializer::deserialize<Fragment>(this->frg, buf, offset);
            serializer::deserialize<u16>(this->wnd, buf, offset);
            serializer::deserialize<u32>(this->ts, buf, offset);
            serializer::deserialize<u32>(this->sn, buf, offset);
            serializer::deserialize<u32>(this->una, buf, offset);
            serializer::deserialize<PayloadLen>(this->len, buf, offset);
        }
    };

    /// SegmentMetadata is used to track the state of the segment in the send queue for (re)transmission purposes.
    struct SegmentMetadata final {
        /// Timestamp for retransmission.
        u32 resendts = 0;

        /// Retransmission timeout.
        u32 rto = 0;

        /// Number of times this segment has been acknowledged without any intervening segments being acknowledged.
        u32 fastack = 0;

        /// Number of times this segment has been transmitted.
        u32 xmit = 0;
    };

    // TODO: Should be used via serializer functions.
    /// SegmentData is used to store the payload of the segment.
    struct SegmentData final {
        std::vector<std::byte> data{};

        explicit SegmentData() = default;
        explicit SegmentData(const size_t size) : data(size) { }

        void assign(const std::span<const std::byte> buf) {
            this->data.assign(buf.begin(), buf.end());
        }

        [[nodiscard]] size_t dynamic_size() const {
            return this->data.size();
        }

        void encode_to(std::span<std::byte> buf, size_t& offset, const size_t length) const {
            assert(buf.size() >= this->data.size());

            std::memcpy(buf.data() + offset, this->data.data(), length);
            offset += length;
        }

        void decode_from(const std::span<const std::byte> buf, size_t& offset, const size_t length) {
            assert(buf.size() >= offset + length);
            this->assign(buf.subspan(offset, length));
            offset += length;
        }

        SegmentData(SegmentData&& other) noexcept {
            this->data = std::move(other.data);
        }

        SegmentData& operator=(SegmentData&& other) noexcept {
            this->data = std::move(other.data);
            return *this;
        }
    };

    // TODO: Should be used via serializer functions.
    struct Segment final {
        static_assert(serializer::fixed_size<SegmentHeader>() == 24, "Segment header is 24 bytes by default. Changes this assert if you know what you're doing.");

        SegmentHeader header{};
        SegmentData data{};
        SegmentMetadata metadata{};

        explicit Segment() = default;
        explicit Segment(const SegmentHeader& header, SegmentData& data) : header(header), data(std::move(data)) { }

        [[nodiscard]] size_t data_size() const {
            return this->header.len.get();
        }

        void data_assign(const std::span<const std::byte> buf) {
            this->data.assign(buf);
            this->header.len = PayloadLen(buf.size());
        }

        Segment(Segment&& other) noexcept {
            this->header = other.header;
            this->data = std::move(other.data);
            this->metadata = other.metadata;
        }

        Segment& operator=(Segment&& other) noexcept {
            this->header = other.header;
            this->data = std::move(other.data);
            this->metadata = other.metadata;
            return *this;
        }
    };
}