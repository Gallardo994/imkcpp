#pragma once

#include "types.hpp"
#include "clock.hpp"
#include "encoder.hpp"
#include "types/conv.hpp"
#include "types/cmd.hpp"
#include "types/fragment.hpp"

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
        timepoint_t ts;

        /// Sequence number.
        u32 sn = 0;

        /// Unacknowledged sequence number.
        u32 una = 0;

        /// Length of the payload.
        PayloadLen len{0};

        constexpr static size_t OVERHEAD =
            encoder::encoded_size<decltype(conv)>() +
            encoder::encoded_size<decltype(cmd)>() +
            encoder::encoded_size<decltype(frg)>() +
            encoder::encoded_size<decltype(wnd)>() +
            encoder::encoded_size<decltype(ts)>() +
            encoder::encoded_size<decltype(sn)>() +
            encoder::encoded_size<decltype(una)>() +
            encoder::encoded_size<decltype(len)>();

        static_assert(OVERHEAD == 24, "SegmentHeader::OVERHEAD is 24 by default. If you really know what you're doing, change this value.");

        void encode_to(std::span<std::byte> buf, size_t& offset) const {
            assert(buf.size() >= SegmentHeader::OVERHEAD);

            encoder::encode<Conv>(buf, offset, this->conv);
            encoder::encode<Cmd>(buf, offset, this->cmd);
            encoder::encode<Fragment>(buf, offset, this->frg);
            encoder::encode<u16>(buf, offset, this->wnd);
            encoder::encode<timepoint_t>(buf, offset, this->ts);
            encoder::encode<u32>(buf, offset, this->sn);
            encoder::encode<u32>(buf, offset, this->una);
            encoder::encode<PayloadLen>(buf, offset, this->len);
        }

        void decode_from(const std::span<const std::byte> buf, size_t& offset) {
            assert(buf.size() >= SegmentHeader::OVERHEAD);

            encoder::decode<Conv>(buf, offset, this->conv);
            encoder::decode<Cmd>(buf, offset, this->cmd);
            encoder::decode<Fragment>(buf, offset, this->frg);
            encoder::decode<u16>(buf, offset, this->wnd);
            encoder::decode<timepoint_t>(buf, offset, this->ts);
            encoder::decode<u32>(buf, offset, this->sn);
            encoder::decode<u32>(buf, offset, this->una);
            encoder::decode<PayloadLen>(buf, offset, this->len);
        }
    };

    /// SegmentMetadata is used to track the state of the segment in the send queue for (re)transmission purposes.
    struct SegmentMetadata final {
        /// Timestamp for retransmission.
        timepoint_t resendts;

        /// Retransmission timeout.
        duration_t rto{0};

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
            assert(buf.size() >= SegmentHeader::OVERHEAD + this->header.len.get());
            assert(this->header.len.get() == this->data.size());

            this->header.encode_to(buf, offset);
            this->data.encode_to(buf, offset, this->header.len.get());
        }

        void decode_from(const std::span<const std::byte> buf, size_t& offset) {
            assert(buf.size() >= SegmentHeader::OVERHEAD + this->header.len.get());

            this->header.decode_from(buf, offset);
            this->data.decode_from(buf, offset, this->header.len.get());
        }

        [[nodiscard]] size_t size() const {
            return SegmentHeader::OVERHEAD + this->data_size();
        }

        [[nodiscard]] size_t data_size() const {
            return this->header.len.get();
        }

        void data_assign(const std::span<const std::byte> buf) {
            this->data.assign(buf);
            this->header.len = PayloadLen(buf.size());
        }
    };
}