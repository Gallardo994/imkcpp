#pragma once

#include "types.hpp"
#include <encoder.hpp>
#include <vector>

namespace imkcpp {
    struct segment {
        u32 conv = 0;
        u32 cmd = 0;
        u32 frg = 0;
        u32 wnd = 0;
        u32 ts = 0;
        u32 sn = 0;
        u32 una = 0;
        u32 resendts = 0;
        u32 rto = 0;
        u32 fastack = 0;
        u32 xmit = 0;
        std::vector<std::byte> data{};

        explicit segment() = default;
        explicit segment(const size_t size) : data(size) { }

        void encode_to(std::vector<std::byte>& buf) const {
            assert(buf.size() >= constants::IKCP_OVERHEAD);

            encoder::encode32u(buf, conv);
            encoder::encode8u(buf, cmd);
            encoder::encode8u(buf, frg);
            encoder::encode16u(buf, wnd);
            encoder::encode32u(buf, ts);
            encoder::encode32u(buf, sn);
            encoder::encode32u(buf, una);
            encoder::encode32u(buf, data.size());

            assert(buf.size() >= constants::IKCP_OVERHEAD + data.size());
            buf.insert(buf.end(), std::begin(data), std::end(data));
        }
    };
}