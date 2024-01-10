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
#include "sender_buffer.hpp"
#include "sender.hpp"
#include "ack_controller.hpp"
#include "flusher.hpp"

namespace imkcpp {
    // TODO: Some day later this should become a template with MTU and max segment size as template parameters
    // TODO: because changing MTU at runtime will cause issues with already queued segments.
    // TODO: Additionally, this will allow more compile-time optimizations.
    class ImKcpp final {
        SharedCtx shared_ctx{};
        Flusher flusher{shared_ctx};
        SenderBuffer sender_buffer{shared_ctx};

        RtoCalculator rto_calculator{shared_ctx};
        CongestionController congestion_controller{flusher, shared_ctx};

        AckController ack_controller{flusher, rto_calculator, sender_buffer, shared_ctx};

        Receiver receiver{congestion_controller, shared_ctx};
        Sender sender{congestion_controller, rto_calculator, flusher, sender_buffer, ack_controller, shared_ctx};

        bool updated = false;
        u32 current = 0; // TODO: Should probably move to shared_ctx as well, but idk
        u32 ts_flush = constants::IKCP_INTERVAL;

        [[nodiscard]] auto create_service_segment(i32 unused_receive_window) const -> Segment;

    public:
        explicit ImKcpp(u32 conv);

        auto set_interval(u32 interval) -> void;
        auto set_nodelay(i32 nodelay) -> void;
        auto set_resend(i32 resend) -> void;
        auto set_mtu(u32 mtu) -> tl::expected<size_t, error>;
        auto set_wndsize(u32 sndwnd, u32 rcvwnd) -> void;
        auto set_congestion_window_enabled(bool state) -> void;

        auto input(std::span<const std::byte> data) -> tl::expected<size_t, error>;
        auto recv(std::span<std::byte> buffer) -> tl::expected<size_t, error>;

        auto send(std::span<const std::byte> buffer) -> tl::expected<size_t, error>;
        auto check(u32 current) -> u32;
        auto update(u32 current, const output_callback_t& callback) -> FlushResult;
        auto flush(const output_callback_t& callback) -> FlushResult;

        [[nodiscard]] auto get_state() const -> State;
        [[nodiscard]] auto get_mtu() const -> u32;
        [[nodiscard]] auto get_max_segment_size() const -> u32;

        [[nodiscard]] auto peek_size() const -> tl::expected<size_t, error>;
        [[nodiscard]] auto estimate_segments_count(size_t size) const -> size_t;
    };
}