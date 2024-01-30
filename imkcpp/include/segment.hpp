#pragma once

#include "types.hpp"
#include "encoder.hpp"
#include "types/conv.hpp"
#include <vector>

namespace imkcpp {
    // TODO: Too many public members, make them private. Also, make the Segment class a friend of the SegmentHeader class.
    // TODO: The logic is heavily reliant on the fact that everything is public. Rework this.

    // TODO: Overhead sizes don't take into account modifications to the types (e.g. Conv is 1 byte, not 4)
    /// SegmentHeader is used to store the header of the segment.
    struct SegmentHeader final {
        /// Conversation ID.
        Conv conv{0};

        /// Command type.
        u8 cmd = 0;

        /// Fragment. Indicates how many next SNs are following this one in the same packet.
        u8 frg = 0;

        /// Window size (available space in the receive buffer).
        u16 wnd = 0;

        /// Timestamp.
        u32 ts = 0;

        /// Sequence number.
        u32 sn = 0;

        /// Unacknowledged sequence number.
        u32 una = 0;

        /// Length of the payload.
        u32 len = 0;

        void encode_to(std::span<std::byte> buf, size_t& offset) const {
            assert(buf.size() >= constants::IKCP_OVERHEAD);

            encoder::encode<Conv>(buf, offset, this->conv);
            encoder::encode<u8>(buf, offset, this->cmd);
            encoder::encode<u8>(buf, offset, this->frg);
            encoder::encode<u16>(buf, offset, this->wnd);
            encoder::encode<u32>(buf, offset, this->ts);
            encoder::encode<u32>(buf, offset, this->sn);
            encoder::encode<u32>(buf, offset, this->una);
            encoder::encode<u32>(buf, offset, this->len);
        }

        void decode_from(const std::span<const std::byte> buf, size_t& offset) {
            assert(buf.size() >= constants::IKCP_OVERHEAD);

            encoder::decode<Conv>(buf, offset, this->conv);
            encoder::decode<u8>(buf, offset, this->cmd);
            encoder::decode<u8>(buf, offset, this->frg);
            encoder::decode<u16>(buf, offset, this->wnd);
            encoder::decode<u32>(buf, offset, this->ts);
            encoder::decode<u32>(buf, offset, this->sn);
            encoder::decode<u32>(buf, offset, this->una);
            encoder::decode<u32>(buf, offset, this->len);
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

    /// SegmentData is used to store the payload of the segment.
    struct SegmentData final {
        std::vector<std::byte> data{};

        explicit SegmentData() = default;
        explicit SegmentData(const size_t size) : data(size) { }

        void assign(const std::span<const std::byte> buf) {
            this->data.assign(buf.begin(), buf.end());
        }

        [[nodiscard]] size_t size() const {
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
    };

    struct Segment final {
        SegmentHeader header{};
        SegmentData data{};
        SegmentMetadata metadata{};

        explicit Segment() = default;
        explicit Segment(const SegmentHeader& header, SegmentData& data) : header(header), data(std::move(data)) { }

        void encode_to(const std::span<std::byte> buf, size_t& offset) const {
            assert(buf.size() >= constants::IKCP_OVERHEAD + this->header.len);
            assert(this->header.len == this->data.size());

            this->header.encode_to(buf, offset);
            this->data.encode_to(buf, offset, this->header.len);
        }

        void decode_from(const std::span<const std::byte> buf, size_t& offset) {
            assert(buf.size() >= constants::IKCP_OVERHEAD + this->header.len);

            this->header.decode_from(buf, offset);
            this->data.decode_from(buf, offset, this->header.len);
        }

        [[nodiscard]] size_t size() const {
            return constants::IKCP_OVERHEAD + this->data_size();
        }

        [[nodiscard]] size_t data_size() const {
            return this->header.len;
        }

        void data_assign(const std::span<const std::byte> buf) {
            this->data.assign(buf);
            this->header.len = buf.size();
        }
    };
}