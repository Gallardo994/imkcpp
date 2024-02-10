#pragma once

#include <algorithm>
#include "types.hpp"
#include "constants.hpp"

namespace imkcpp {
    /// RtoCalculator is used to calculate retransmission timeout. It's based on RFC 2988.
    class RtoCalculator final {
        /// Interval aka G in RFC 2988
        duration_t interval = 0ms;

        /// Smoothed round trip time aka SRTT in RFC 2988
        duration_t srtt = 0ms;

        /// Round trip time variation aka RTTVAR in RFC 2988
        duration_t rttvar = 0ms;

        /// Retransmission timeout aka RTO in RFC 2988
        duration_t rto = milliseconds_t(constants::IKCP_RTO_DEF);

        /// Last measured round trip time
        duration_t last_rtt = 0ms;

        /// Minimum retransmission timeout aka RTO_MIN in RFC 2988
        duration_t minrto = milliseconds_t(constants::IKCP_RTO_MIN);

        /// Maximum retransmission timeout aka RTO_MAX in RFC 2988
        duration_t maxrto = milliseconds_t(constants::IKCP_RTO_MAX);

    public:
        void set_interval(const duration_t interval) {
            this->interval = interval;
        }

        void update_rto(const timepoint_t current, const timepoint_t ts) {
            const auto rtt = current - ts;

            if (rtt.count() < 0) {
                return;
            }

            this->last_rtt = rtt;

            if (this->srtt.count() == 0) {
                // First measurement
                this->srtt = rtt;
                this->rttvar = rtt / 2;
            } else {
                // Consequent measurements
                const auto delta = duration_t(abs(rtt.count() - srtt.count()));

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
            const duration_t rto = this->srtt + std::max(this->interval, K * this->rttvar);

            // Limiting to IKCP_RTO_MAX is not mentioned in RFC 2988, but is present in the original C implementation
            this->rto = std::clamp(rto, this->minrto, this->maxrto);
        }

        void set_min_rto(const duration_t minrto) {
            this->minrto = minrto;
        }

        [[nodiscard]] duration_t get_rto() const {
            return this->rto;
        }

        [[nodiscard]] duration_t get_last_rtt() const {
            return this->last_rtt;
        }
    };
}
