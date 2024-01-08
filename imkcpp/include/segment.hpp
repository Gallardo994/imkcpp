#pragma once

#include "types.hpp"
#include "encoder.hpp"
#include <vector>

namespace imkcpp {
    struct SegmentHeader {
        u32 conv = 0;
        u8 cmd = 0;
        u8 frg = 0;
        u16 wnd = 0;
        u32 ts = 0;
        u32 sn = 0;
        u32 una = 0;
        u32 len = 0;

        void encode_to(std::span<std::byte>& buf, size_t& offset) const {
            assert(buf.size() >= constants::IKCP_OVERHEAD);

            encoder::encode32u(buf, offset, conv);
            encoder::encode8u(buf, offset, cmd);
            encoder::encode8u(buf, offset, frg);
            encoder::encode16u(buf, offset, wnd);
            encoder::encode32u(buf, offset, ts);
            encoder::encode32u(buf, offset, sn);
            encoder::encode32u(buf, offset, una);
            encoder::encode32u(buf, offset, len);
        }

        void decode_from(const std::span<const std::byte> buf, size_t& offset) {
            assert(buf.size() >= constants::IKCP_OVERHEAD);

            encoder::decode32u(buf, offset, conv);
            encoder::decode8u(buf, offset, cmd);
            encoder::decode8u(buf, offset, frg);
            encoder::decode16u(buf, offset, wnd);
            encoder::decode32u(buf, offset, ts);
            encoder::decode32u(buf, offset, sn);
            encoder::decode32u(buf, offset, una);
            encoder::decode32u(buf, offset, len);
        }
    };

    struct SegmentMetadata {
        u32 resendts = 0;
        u32 rto = 0;
        u32 fastack = 0;
        u32 xmit = 0;
    };

    struct SegmentData {
        std::vector<std::byte> data{};

        explicit SegmentData() = default;
        explicit SegmentData(const size_t size) : data(size) { }

        void assign(const std::span<const std::byte> buf) {
            data.assign(buf.begin(), buf.end());
        }

        void encode_to(std::span<std::byte>& buf, size_t& offset, const size_t length) const {
            assert(buf.size() >= data.size());

            std::memcpy(buf.data() + offset, data.data(), length);
            offset += length;
        }

        void decode_from(const std::span<const std::byte> buf, size_t& offset, const size_t length) {
            assert(buf.size() >= data.size());

            std::memcpy(data.data(), buf.data() + offset, length);
            offset += length;
        }
    };

    struct Segment {
        SegmentHeader header{};
        SegmentMetadata metadata{};
        SegmentData data{};

        explicit Segment() = default;

        void encode_to(std::span<std::byte> buf, size_t& offset) const {
            assert(buf.size() >= constants::IKCP_OVERHEAD + header.len);
            assert(header.len == data.data.size());

            header.encode_to(buf, offset);
            data.encode_to(buf, offset, header.len);
        }

        void decode_from(const std::span<const std::byte> buf, size_t& offset) {
            assert(buf.size() >= constants::IKCP_OVERHEAD + header.len);

            header.decode_from(buf, offset);
            data.decode_from(buf, offset, header.len);
        }

        [[nodiscard]] size_t size() const {
            return constants::IKCP_OVERHEAD + header.len;
        }

        [[nodiscard]] size_t data_size() const {
            return header.len;
        }
    };
}