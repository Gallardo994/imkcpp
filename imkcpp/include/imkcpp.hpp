#pragma once

#include <cstdint>
#include <functional>
#include <span>
#include <queue>
#include <optional>
#include <cstddef>

#include "segment.hpp"

class imkcpp final {
private:
    uint32_t conv, mtu, mss, state;
    uint32_t snd_una, snd_nxt, rcv_nxt;
    uint32_t ts_recent, ts_lastack, ssthresh;
    uint32_t rx_rttval, rx_srtt, rx_rto, rx_minrto;
    uint32_t snd_wnd, rcv_wnd, rmt_wnd, cwnd, probe;
    uint32_t current, interval, ts_flush, xmit;
    uint32_t nrcv_buf, nsnd_buf;
    uint32_t nrcv_que, nsnd_que;
    uint32_t nodelay, updated;
    uint32_t ts_probe, probe_wait;
    uint32_t dead_link, incr;

    std::deque<segment> snd_queue;
    std::deque<segment> rcv_queue;
    std::deque<segment> snd_buf;
    std::deque<segment> rcv_buf;

    uint32_t* acklist;
    uint32_t ackcount;
    uint32_t ackblock;

    std::optional<void*> user;
    std::vector<std::byte> buffer;

    int fastresend;
    int fastlimit;
    int nocwnd, stream;

    std::function<int(std::span<const std::byte> data, const imkcpp& imkcpp, std::optional<void*> user)> output;

public:
    void set_output(const std::function<int(std::span<const std::byte> data, const imkcpp& imkcpp, std::optional<void*> user)>& output);
    void set_interval(uint32_t interval);
    void set_nodelay(int32_t nodelay, uint32_t interval, int32_t resend, int32_t nc);
    void set_mtu(uint32_t mtu);
    void set_wndsize(uint32_t sndwnd, uint32_t rcvwnd);

    [[nodiscard]] int peek_size() const;
    void update_ack(int32_t rtt);
    int32_t recv(std::span<std::byte>& buffer);
};