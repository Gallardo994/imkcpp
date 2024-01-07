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

namespace imkcpp {
    enum class State : i32 {
        Alive = 0,
        DeadLink = 1,
    };

    class ImKcpp final {
    private:
        u32 conv = 0;
        size_t mtu = 0;
        size_t mss = 0;

        State state = State::Alive;

        u32 snd_una = 0;
        u32 snd_nxt = 0;
        u32 rcv_nxt = 0;
        u32 ssthresh = constants::IKCP_THRESH_INIT;
        u32 rx_rttval = 0;
        u32 rx_srtt = 0;
        u32 rx_rto = constants::IKCP_RTO_DEF;
        u32 rx_minrto = constants::IKCP_RTO_MIN;
        u32 snd_wnd = constants::IKCP_WND_SND;
        u32 rcv_wnd = constants::IKCP_WND_RCV;
        u32 rmt_wnd = constants::IKCP_WND_RCV;
        u32 cwnd = 0;
        u32 probe = 0;
        u32 current = 0;
        u32 interval = constants::IKCP_INTERVAL;
        u32 ts_flush = constants::IKCP_INTERVAL;
        u32 xmit = 0;
        u32 nodelay = 0;
        bool updated = false;
        u32 ts_probe = 0;
        u32 probe_wait = 0;
        u32 dead_link = constants::IKCP_DEADLINK;
        u32 incr = 0;

        std::deque<segment> snd_queue{};
        std::deque<segment> rcv_queue{};
        std::deque<segment> snd_buf{};
        std::deque<segment> rcv_buf{};

        std::vector<Ack> acklist{};

        std::optional<void*> user = std::nullopt;
        std::vector<std::byte> buffer{};

        i32 fastresend = 0;
        i32 fastlimit = constants::IKCP_FASTACK_LIMIT;
        i32 nocwnd = 0;

        std::function<i32(std::span<const std::byte> data, const ImKcpp& imkcpp, std::optional<void*> user)> output;

        void call_output(const std::span<const std::byte>& data) const;
        void update_ack(i32 rtt);
        void shrink_buf();
        void parse_ack(u32 sn);
        void parse_una(u32 una);
        void parse_fastack(u32 sn, u32 ts);
        void ack_push(u32 sn, u32 ts);
        [[nodiscard]] std::optional<Ack> ack_get(size_t p) const;
        void parse_data(const segment& newseg);
        [[nodiscard]] i32 wnd_unused() const;

    public:
        explicit ImKcpp(u32 conv, std::optional<void*> user = std::nullopt);

        void set_output(const std::function<int(std::span<const std::byte> data, const ImKcpp& imkcpp, std::optional<void*> user)>& output);
        void set_interval(u32 interval);
        void set_nodelay(i32 nodelay, u32 interval, i32 resend, i32 nc);
        void set_mtu(u32 mtu);
        void set_wndsize(u32 sndwnd, u32 rcvwnd);

        i32 recv(std::span<std::byte>& buffer);
        i32 send(const std::span<const std::byte>& buffer);
        i32 input(const std::span<const std::byte>& data);
        void update(u32 current);
        u32 check(u32 current);
        void flush();

        [[nodiscard]] u32 get_mtu() const;
        [[nodiscard]] u32 get_max_segment_size() const;
        [[nodiscard]] i32 peek_size() const;
    };
}