#pragma once

#include <algorithm>
#include "types.hpp"
#include "constants.hpp"
#include "shared_ctx.hpp"

namespace imkcpp {
    // https://www.computer-networking.info/1st/html/transport/tcp.html
    // https://datatracker.ietf.org/doc/html/rfc2988.html#section-2
    class RtoCalculator {
        SharedCtx& shared_ctx;

        u32 srtt = 0; // Smoothed round trip time
        u32 rttvar = 0; // Round trip time variation
        u32 rto = constants::IKCP_RTO_DEF; // Retransmission timeout

        u32 minrto = constants::IKCP_RTO_MIN; // Minimum retransmission timeout
        u32 maxrto = constants::IKCP_RTO_MAX; // Maximum retransmission timeout

    public:
        u32 last_rtt = 0;

        explicit RtoCalculator(SharedCtx& shared_ctx) : shared_ctx(shared_ctx) {}

        void update_rtt(const i32 rtt) {
            this->last_rtt = rtt;

            if (this->srtt == 0) {
                // First measurement
                this->srtt = rtt;
                this->rttvar = rtt / 2;
            } else {
                // Consequent measurements
                const i32 delta = std::abs(rtt - static_cast<i32>(this->srtt));

                // RTTVAR = (1 - BETA) * RTTVAR + BETA * |SRTT - RTT|
                // Simplified to: RTTVAR = (BETA_MUL * RTTVAR + delta) / BETA_DIV
                // In RFC 2988, BETA = 1/4
                constexpr u32 BETA_MUL = 3;
                constexpr u32 BETA_DIV = 4;
                this->rttvar = (BETA_MUL * this->rttvar + delta) / BETA_DIV;

                // SRTT = (1 - ALPHA) * SRTT + ALPHA * RTT
                // Simplified to: SRTT = (ALPHA_MUL * SRTT + RTT) / ALPHA_DIV
                // In RFC 2988, ALPHA = 1/8
                constexpr u32 ALPHA_MUL = 7;
                constexpr u32 ALPHA_DIV = 8;
                this->srtt = (ALPHA_MUL * this->srtt + rtt) / ALPHA_DIV;
            }

            // RTO = SRTT + max(G, K * RTTVAR)
            constexpr u32 K = 4;
            const u32 rto = this->srtt + std::max(this->shared_ctx.get_interval(), K * this->rttvar);

            // Limiting to IKCP_RTO_MAX is not mentioned in RFC 2988, but is present in the original C implementation
            // TODO: Investigate why it's needed
            this->rto = std::clamp(rto, this->minrto, this->maxrto);
        }

        void set_min_rto(const u32 minrto) {
            this->minrto = minrto;
        }

        [[nodiscard]] u32 get_rto() const {
            return this->rto;
        }
    };
}
