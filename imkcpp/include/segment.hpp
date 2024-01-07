#pragma once

#include "types.hpp"
#include <encoder.hpp>
#include <vector>

namespace imkcpp {
    struct segment {
        u32 conv, cmd, frg;
        u32 wnd, ts, sn, una;
        u32 resendts, rto;
        u32 fastack, xmit;
        std::vector<std::byte> data;

        explicit segment() : conv(0), cmd(0), frg(0),
                             wnd(0), ts(0), sn(0), una(0),
                             resendts(0), rto(0),
                             fastack(0), xmit(0) {
        }

        explicit segment(const size_t size) : conv(0), cmd(0), frg(0),
                                              wnd(0), ts(0), sn(0), una(0),
                                              resendts(0), rto(0),
                                              fastack(0), xmit(0),
                                              data(size) {
        }

        void encode_header_to(std::vector<std::byte>& buf) const {
            assert(buf.size() >= constants::IKCP_OVERHEAD);

            encoder::encode32u(buf, conv);
            encoder::encode8u(buf, cmd);
            encoder::encode8u(buf, frg);
            encoder::encode16u(buf, wnd);
            encoder::encode32u(buf, ts);
            encoder::encode32u(buf, sn);
            encoder::encode32u(buf, una);
            encoder::encode32u(buf, data.size());
        }

        void encode_data_to(std::vector<std::byte>& buf) const {
            if (data.empty()) {
                return;
            }

            assert(buf.size() >= constants::IKCP_OVERHEAD + data.size());

            buf.insert(buf.end(), std::begin(data), std::end(data));
        }
    };
}