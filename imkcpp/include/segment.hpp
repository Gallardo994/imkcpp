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

        void encode_to(std::span<std::byte> buf, size_t& offset) const {
            assert(buf.size() >= constants::IKCP_OVERHEAD);

            encoder::encode32u(buf, offset, this->conv);
            encoder::encode8u(buf, offset, this->cmd);
            encoder::encode8u(buf, offset, this->frg);
            encoder::encode16u(buf, offset, this->wnd);
            encoder::encode32u(buf, offset, this->ts);
            encoder::encode32u(buf, offset, this->sn);
            encoder::encode32u(buf, offset, this->una);
            encoder::encode32u(buf, offset, this->len);
        }

        void decode_from(const std::span<const std::byte> buf, size_t& offset) {
            assert(buf.size() >= constants::IKCP_OVERHEAD);

            encoder::decode32u(buf, offset, this->conv);
            encoder::decode8u(buf, offset, this->cmd);
            encoder::decode8u(buf, offset, this->frg);
            encoder::decode16u(buf, offset, this->wnd);
            encoder::decode32u(buf, offset, this->ts);
            encoder::decode32u(buf, offset, this->sn);
            encoder::decode32u(buf, offset, this->una);
            encoder::decode32u(buf, offset, this->len);
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
            this->data.assign(buf.begin(), buf.end());
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

    struct Segment {
        SegmentHeader header{};
        SegmentMetadata metadata{};
        SegmentData data{};

        explicit Segment() = default;

        void encode_to(const std::span<std::byte> buf, size_t& offset) const {
            assert(buf.size() >= constants::IKCP_OVERHEAD + header.len);
            assert(this->header.len == this->data.data.size());

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