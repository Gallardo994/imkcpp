#pragma once

#include "types.hpp"
#include "constants.hpp"
#include "utility.hpp"

namespace imkcpp {
    enum class ProbeFlag : u32 {
        AskSend = 0x1,
        AskTell = 0x2,
    };

    class WindowProber final {
        constexpr static duration_t PROBE_INIT = 7000ms; // 7 secs to probe window size
        constexpr static duration_t PROBE_LIMIT = 120000ms; // up to 120 secs to probe window

        /// Probe flags
        u32 probe = 0; // Probe flags

        /// Timestamp of the last time we probed the remote window
        timepoint_t ts_probe; // Timestamp of the last time we probed the remote window

        /// How long we should wait before probing again
        duration_t probe_wait{0}; // How long we should wait before probing again

    public:
        void update(const timepoint_t current, const u32 rmt_wnd) {
            if (rmt_wnd != 0) {
                this->ts_probe = timepoint_t();
                this->probe_wait = duration_t{0};

                return;
            }

            if (this->probe_wait == duration_t{0}) {
                this->probe_wait = PROBE_INIT;
                this->ts_probe = current + this->probe_wait;
            } else {
                if (current >= this->ts_probe) {
                    if (this->probe_wait < PROBE_INIT) {
                        this->probe_wait = PROBE_INIT;
                    }

                    this->probe_wait += this->probe_wait / 2;
                    if (this->probe_wait > PROBE_LIMIT) {
                        this->probe_wait = PROBE_LIMIT;
                    }

                    this->ts_probe = current + this->probe_wait;

                    this->set_flag(ProbeFlag::AskSend);
                }
            }
        }

        void set_flag(const ProbeFlag flag) {
            assert(flag >= ProbeFlag::AskSend && flag <= ProbeFlag::AskTell);

            this->probe |= static_cast<u32>(flag);
        }

        [[nodiscard]] bool has_flag(const ProbeFlag flag) const {
            assert(flag >= ProbeFlag::AskSend && flag <= ProbeFlag::AskTell);

            return (this->probe & static_cast<u32>(flag)) != 0;
        }

        void reset_flags() {
            this->probe = 0;
        }
    };
}