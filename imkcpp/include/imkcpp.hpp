#pragma once

#include <functional>
#include <span>
#include <queue>
#include <optional>
#include <cstddef>

#include "types.hpp"
#include "constants.hpp"
#include "segment.hpp"
#include "state.hpp"
#include "expected.hpp"
#include "errors.hpp"
#include "results.hpp"
#include "rto_calculator.hpp"
#include "congestion_controller.hpp"
#include "shared_ctx.hpp"
#include "receiver.hpp"
#include "sender.hpp"
#include "ack_controller.hpp"
#include "flusher.hpp"

namespace imkcpp {
    // TODO: Some day later this should become a template with MTU and max segment size as template parameters
    // TODO: because changing MTU at runtime will cause issues with already queued segments.
    // TODO: Additionally, this will allow more compile-time optimizations.
    class ImKcpp final {
    private:
        SharedCtx shared_ctx{};
        Flusher flusher{shared_ctx};

        RtoCalculator rto_calculator{};
        CongestionController congestion_controller{flusher};

        AckController ack_controller{flusher, shared_ctx};

        Receiver receiver{congestion_controller, shared_ctx};
        Sender sender{congestion_controller, rto_calculator, flusher, shared_ctx};

        // TODO: This needs to be split into receiver and sender, and maybe shared context part

        // TODO: And for the god's sake, rename all these variables to something more meaningful

        u32 current = 0;
        u32 interval = constants::IKCP_INTERVAL;
        u32 ts_flush = constants::IKCP_INTERVAL;

        bool updated = false;

        [[nodiscard]] Segment create_service_segment(i32 unused_receive_window) const;

    public:
        explicit ImKcpp(u32 conv);

        [[nodiscard]] State get_state() const;
        void set_interval(u32 interval);
        void set_nodelay(i32 nodelay);
        void set_resend(i32 resend);
        tl::expected<size_t, error> set_mtu(u32 mtu);
        void set_wndsize(u32 sndwnd, u32 rcvwnd);
        void set_congestion_window_enabled(bool state);

        tl::expected<size_t, error> recv(std::span<std::byte> buffer);
        [[nodiscard]] size_t estimate_segments_count(size_t size) const;
        tl::expected<size_t, error> send(std::span<const std::byte> buffer);
        tl::expected<size_t, error> input(std::span<const std::byte> data);
        FlushResult update(u32 current, const output_callback_t& callback);
        u32 check(u32 current);
        FlushResult flush(const output_callback_t& callback);

        [[nodiscard]] u32 get_mtu() const;
        [[nodiscard]] u32 get_max_segment_size() const;
        [[nodiscard]] tl::expected<size_t, error> peek_size() const;
    };
}