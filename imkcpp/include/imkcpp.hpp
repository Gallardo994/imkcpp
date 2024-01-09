#pragma once

#include <functional>
#include <span>
#include <queue>
#include <optional>
#include <cstddef>

#include "types.hpp"
#include "constants.hpp"
#include "segment.hpp"
#include "ack.hpp"
#include "state.hpp"
#include "expected.hpp"
#include "errors.hpp"
#include "results.hpp"
#include "rto_calculator.hpp"
#include "congestion_controller.hpp"
#include "shared_ctx.hpp"
#include "receiver.hpp"

namespace imkcpp {
    // TODO: Some day later this should become a template with MTU and max segment size as template parameters
    // TODO: because changing MTU at runtime will cause issues with already queued segments.
    // TODO: Additionally, this will allow more compile-time optimizations.
    class ImKcpp final {
    private:
        // TODO: This never needs to be a full 32-bit value. 16 or even 8 bits should be enough.
        // TODO: We must never trust this value from the other side anyway. So the only useful application
        // TODO: is distinguishing between different "channels" from the same peer.
        u32 conv = 0;
        size_t mtu = 0;
        size_t mss = 0;

        State state = State::Alive;
        SharedCtx shared_ctx{};
        RtoCalculator rto_calculator{};
        CongestionController congestion_controller{};
        Receiver receiver{congestion_controller, shared_ctx};

        // TODO: This needs to be split into receiver and sender, and maybe shared context part

        // TODO: And for the god's sake, rename all these variables to something more meaningful

        u32 current = 0;
        u32 interval = constants::IKCP_INTERVAL;
        u32 ts_flush = constants::IKCP_INTERVAL;
        u32 xmit = 0;
        u32 nodelay = 0;
        bool updated = false;

        u32 dead_link = constants::IKCP_DEADLINK;

        std::deque<Segment> snd_queue{};
        std::deque<Segment> snd_buf{};

        std::vector<Ack> acklist{};

        std::vector<std::byte> buffer{};

        i32 fastresend = 0;
        i32 fastlimit = constants::IKCP_FASTACK_LIMIT;

        void shrink_buf();
        void parse_ack(u32 sn);
        void parse_una(u32 una);
        void parse_fastack(u32 sn, u32 ts);
        [[nodiscard]] std::optional<Ack> ack_get(size_t p) const;
        void move_send_queue_to_buffer(u32 cwnd, u32 current, i32 unused_receive_window);

    public:
        explicit ImKcpp(u32 conv);

        [[nodiscard]] State get_state() const;
        void set_output(const output_callback_t& output);
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