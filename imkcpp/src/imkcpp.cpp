#include "imkcpp.hpp"
#include "constants.hpp"
#include <cstring>
#include <cassert>

void imkcpp::set_output(const std::function<i32(std::span<const std::byte> data, const imkcpp& imkcpp, std::optional<void*> user)>&) {
    this->output = output;
}

void imkcpp::set_interval(const u32 interval) {
    this->interval = interval;
}

void imkcpp::set_nodelay(const i32 nodelay, u32 interval, const i32 resend, const i32 nc) {
    if (nodelay >= 0) {
        this->nodelay = nodelay;

        if (nodelay) {
            this->rx_minrto = IKCP_RTO_NDL;
        }
        else {
            this->rx_minrto = IKCP_RTO_MIN;
        }
    }

    this->interval = std::clamp(interval, 10U, 5000U);

    if (resend >= 0) {
        this->fastresend = resend;
    }

    if (nc >= 0) {
        this->nocwnd = nc;
    }
}

void imkcpp::set_mtu(const u32 mtu) {
    if (mtu <= IKCP_OVERHEAD) {
        return;
    }

    this->buffer.resize(static_cast<size_t>(mtu + IKCP_OVERHEAD) * 3);
    this->mtu = mtu;
    this->mss = this->mtu - IKCP_OVERHEAD;
}

void imkcpp::set_wndsize(const u32 sndwnd, const u32 rcvwnd) {
    if (sndwnd > 0) {
        this->snd_wnd = sndwnd;
    }

    if (rcvwnd > 0) {
        this->rcv_wnd = std::max(rcvwnd, IKCP_WND_RCV);
    }
}

i32 imkcpp::peek_size() const {
    if (this->rcv_queue.empty()) {
        return -1;
    }

    const auto front = this->rcv_queue.front();

    if (front.frg == 0) {
        return static_cast<int>(front.data.size());
    }

    if (this->rcv_queue.size() < front.frg + 1) {
        return -1;
    }

    int length = 0;
    for (const auto& seg : this->rcv_queue) {
        length += static_cast<int>(seg.data.size());

        if (seg.frg == 0) {
            break;
        }
    }

    return length;
}

// Parse ack

void imkcpp::update_ack(const i32 rtt) {
    if (this->rx_srtt == 0) {
        this->rx_srtt = rtt;
        this->rx_rttval = rtt / 2;
    }
    else {
        i32 delta = rtt - static_cast<i32>(this->rx_srtt);
        delta = delta >= 0 ? delta : -delta;

        this->rx_rttval = (3 * this->rx_rttval + delta) / 4;
        this->rx_srtt = (7 * this->rx_srtt + rtt) / 8;

        if (this->rx_srtt < 1) {
            this->rx_srtt = 1;
        }
    }

    const u32 rto = this->rx_srtt + std::max(this->interval, 4 * this->rx_rttval);
    this->rx_rto = std::clamp(rto, this->rx_minrto, IKCP_RTO_MAX);
}

// TODO: ikcp_shrink_buf

// TODO: ikcp_parse_ack

// TODO: ikcp_parse_una

// TODO: ikcp_parse_fastack

// receive

i32 imkcpp::recv(std::span<std::byte>& buffer) {
    if (this->rcv_queue.empty()) {
        return -1;
    }

    const int peeksize = this->peek_size();

    if (peeksize < 0) {
        return -2;
    }

    if (peeksize > buffer.size()) {
        return -3;
    }

    const bool recover = nrcv_que >= rcv_wnd;

    size_t len = 0;
    for (auto it = rcv_queue.begin(); it != rcv_queue.end();) {
        const segment& segment = *it;
        const size_t copy_len = std::min(segment.data.size(), buffer.size() - len);
        std::memcpy(buffer.data() + len, segment.data.data(), copy_len);
        len += copy_len;

        it = rcv_queue.erase(it);
        nrcv_que--;

        if (segment.frg == 0) {
            break;
        }

        if (len >= peeksize) {
            break;
        }
    }

    assert(len == peeksize);

    while (!rcv_buf.empty()) {
        auto& seg = rcv_buf.front();
        if (seg.sn != rcv_nxt || nrcv_que >= rcv_wnd) {
            break;
        }

        rcv_queue.push_back(std::move(seg));
        rcv_buf.pop_front();
        nrcv_que++;
        rcv_nxt++;
    }

    if (nrcv_que < rcv_wnd && recover) {
        this->probe |= IKCP_ASK_TELL;
    }

    return static_cast<i32>(len);
}

// send

i32 imkcpp::send(const std::span<const std::byte>& buffer) {
    assert(mss > 0);
    if (buffer.empty()) {
        return -1;
    }

    int len = buffer.size();
    int sent = 0;
    auto buf_ptr = buffer.begin();

    if (stream != 0 && !snd_queue.empty()) {
        segment& old = snd_queue.back();
        if (old.data.size() < static_cast<size_t>(mss)) {
            int capacity = mss - old.data.size();
            int extend = std::min(len, capacity);

            segment seg(old.data.size() + extend);
            std::copy(old.data.begin(), old.data.end(), seg.data.begin());
            std::copy(buf_ptr, buf_ptr + extend, seg.data.begin() + old.data.size());

            seg.len = old.data.size() + extend;
            seg.frg = 0;

            len -= extend;
            buf_ptr += extend;
            sent = extend;

            snd_queue.pop_back();
            snd_queue.push_back(seg);
        }
    }

    if (len <= 0) {
        return sent;
    }

    int count = (len <= mss) ? 1 : (len + mss - 1) / mss;

    if (count >= IKCP_WND_RCV) {
        if (stream != 0 && sent > 0) {
            return sent;
        }

        return -2;
    }

    count = std::max(count, 1);

    for (int i = 0; i < count; i++) {
        int size = std::min(len, static_cast<int32_t>(mss));
        segment seg(size);
        std::copy(buf_ptr, buf_ptr + size, seg.data.begin());

        seg.len = size;
        seg.frg = (stream == 0) ? (count - i - 1) : 0;

        snd_queue.push_back(seg);
        nsnd_que++;

        buf_ptr += size;
        len -= size;
        sent += size;
    }

    return sent;
}