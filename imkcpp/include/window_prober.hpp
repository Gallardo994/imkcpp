#pragma once

#include "types.hpp"
#include "constants.hpp"
#include "utility.hpp"

namespace imkcpp {
    class WindowProber final {
        /// Probe flags
        u32 probe = 0;

        /// Timestamp of the last time we probed the remote window
        u32 ts_probe = 0; // Timestamp of the last time we probed the remote window

        /// How long we should wait before probing again
        u32 probe_wait = 0; // How long we should wait before probing again

    public:
        void update(const u32 current, const u32 rmt_wnd) {
            if (rmt_wnd != 0) {
                this->ts_probe = 0;
                this->probe_wait = 0;

                return;
            }

            if (this->probe_wait == 0) {
                this->probe_wait = constants::IKCP_PROBE_INIT;
                this->ts_probe = current + this->probe_wait;
            } else {
                if (time_delta(current, this->ts_probe) >= 0) {
                    if (this->probe_wait < constants::IKCP_PROBE_INIT) {
                        this->probe_wait = constants::IKCP_PROBE_INIT;
                    }

                    this->probe_wait += this->probe_wait / 2;
                    if (this->probe_wait > constants::IKCP_PROBE_LIMIT) {
                        this->probe_wait = constants::IKCP_PROBE_LIMIT;
                    }

                    this->ts_probe = current + this->probe_wait;

                    this->set_flag(constants::IKCP_ASK_SEND);
                }
            }
        }

        void set_flag(const u32 flag) {
            this->probe |= flag;
        }

        [[nodiscard]] bool has_flag(const u32 flag) const {
            return (this->probe & flag) != 0;
        }

        void reset_flags() {
            this->probe = 0;
        }
    };
}