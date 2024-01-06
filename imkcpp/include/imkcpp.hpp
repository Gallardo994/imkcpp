#pragma once

#include <cstdint>
#include <functional>
#include <span>
#include <queue>
#include <optional>
#include <cstddef>

#include "types.hpp"
#include "segment.hpp"
#include "ack.hpp"

class imkcpp final {
private:
    u32 conv, mtu, mss, state;
    u32 snd_una, snd_nxt, rcv_nxt;
    u32 ts_recent, ts_lastack, ssthresh;
    u32 rx_rttval, rx_srtt, rx_rto, rx_minrto;
    u32 snd_wnd, rcv_wnd, rmt_wnd, cwnd, probe;
    u32 current, interval, ts_flush, xmit;
    u32 nrcv_buf, nsnd_buf;
    u32 nrcv_que, nsnd_que;
    u32 nodelay, updated;
    u32 ts_probe, probe_wait;
    u32 dead_link, incr;

    std::deque<segment> snd_queue;
    std::deque<segment> rcv_queue;
    std::deque<segment> snd_buf;
    std::deque<segment> rcv_buf;

    std::vector<Ack> acklist;
    u32 ackcount;
    u32 ackblock;

    std::optional<void*> user;
    std::vector<std::byte> buffer;

    i32 fastresend;
    i32 fastlimit;
    i32 nocwnd, stream;

    std::function<i32(std::span<const std::byte> data, const imkcpp& imkcpp, std::optional<void*> user)> output;

public:
    void set_output(const std::function<int(std::span<const std::byte> data, const imkcpp& imkcpp, std::optional<void*> user)>& output);
    void set_interval(u32 interval);
    void set_nodelay(i32 nodelay, u32 interval, i32 resend, i32 nc);
    void set_mtu(u32 mtu);
    void set_wndsize(u32 sndwnd, u32 rcvwnd);

    [[nodiscard]] i32 peek_size() const;
    void update_ack(i32 rtt);
    void shrink_buf();
    void parse_ack(u32 sn);
    void parse_una(u32 una);
    void parse_fastack(u32 sn);
    void ack_push(u32 sn, u32 ts);
    [[nodiscard]] std::optional<Ack> ack_get(int p) const;
    i32 recv(std::span<std::byte>& buffer);
    i32 send(const std::span<const std::byte>& buffer);
};