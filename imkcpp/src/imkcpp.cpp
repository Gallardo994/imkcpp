#include <cstring>
#include <cassert>

#include "imkcpp.hpp"
#include "constants.hpp"
#include "encoder.hpp"

static int _itimediff(unsigned long later, unsigned long earlier) {
    return static_cast<int>(later - earlier);
}

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

void imkcpp::shrink_buf() {
    if (!this->snd_buf.empty()) {
        const auto& seg = this->snd_buf.front();
        this->snd_una = seg.sn;
    }
    else {
        this->snd_una = this->snd_nxt;
    }
}

void imkcpp::parse_ack(const u32 sn) {
    if (_itimediff(sn, this->snd_una) < 0 || _itimediff(sn, this->snd_nxt) >= 0) {
        return;
    }

    for (auto it = this->snd_buf.begin(); it != this->snd_buf.end();) {
        if (sn == it->sn) {
            snd_buf.erase(it);
            this->nsnd_buf--;
            break;
        }

        if (_itimediff(sn, it->sn) < 0) {
            break;
        }

        ++it;
    }
}

void imkcpp::parse_una(const u32 una) {
    for (auto it = this->snd_buf.begin(); it != this->snd_buf.end();) {
        if (_itimediff(una, it->sn) > 0) {
            it = this->snd_buf.erase(it);
            this->nsnd_buf--;
        }
        else {
            break;
        }
    }
}

void imkcpp::parse_fastack(const u32 sn, const u32 ts) {
    if (_itimediff(sn, snd_una) < 0 || _itimediff(sn, snd_nxt) >= 0) {
        return;
    }

    for (auto& seg : snd_buf) {
        if (_itimediff(sn, seg.sn) < 0) {
            break;
        } else if (sn != seg.sn) {
#ifndef IKCP_FASTACK_CONSERVE
            seg.fastack++;
#else
            if (_itimediff(ts, seg.ts) >= 0) {
                seg.fastack++;
            }
#endif
        }
    }
}

// ack append

void imkcpp::ack_push(const u32 sn, const u32 ts) {
    this->acklist.emplace_back(sn, ts);
}

std::optional<Ack> imkcpp::ack_get(const int p) const {
    if (p < 0 || static_cast<size_t>(p) >= this->acklist.size()) {
        return std::nullopt;
    }

    return this->acklist[p];
}

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

void imkcpp::parse_data(const segment& newseg) {
    u32 sn = newseg.sn;
    bool repeat = false;

    if (_itimediff(sn, rcv_nxt + rcv_wnd) >= 0 || _itimediff(sn, rcv_nxt) < 0) {
        return;
    }

    const auto it = std::find_if(rcv_buf.rbegin(), rcv_buf.rend(), [sn](const segment& seg) {
        return _itimediff(sn, seg.sn) <= 0;
    });

    if (it != rcv_buf.rend() && it->sn == sn) {
        repeat = true;
    }

    if (!repeat) {
        rcv_buf.insert(it.base(), newseg);
        nrcv_buf++;
    }

    // Move available data from rcv_buf to rcv_queue
    while (!rcv_buf.empty()) {
        segment &seg = rcv_buf.front();
        if (seg.sn == rcv_nxt && nrcv_que < static_cast<size_t>(rcv_wnd)) {
            rcv_buf.pop_front();
            rcv_queue.push_back(seg);
            nrcv_que++;
            rcv_nxt++;
            nrcv_buf--;
        } else {
            break;
        }
    }
}

std::byte* imkcpp::encode_seg(std::byte* ptr, const segment& seg) {
    using namespace encoder;

    ptr = encode32u(ptr, seg.conv);
    ptr = encode8u(ptr, seg.cmd);
    ptr = encode8u(ptr, seg.frg);
    ptr = encode16u(ptr, seg.wnd);
    ptr = encode32u(ptr, seg.ts);
    ptr = encode32u(ptr, seg.sn);
    ptr = encode32u(ptr, seg.una);
    ptr = encode32u(ptr, seg.len);

    return ptr;
}

i32 imkcpp::input(const std::span<const std::byte>& data) {
    if (data.size() < IKCP_OVERHEAD) {
        return -1;
    }

    i32 prev_una = this->snd_una;
    i32 maxack = 0, latest_ts = 0;
    int flag = 0;

    const std::byte* ptr = data.data();
    const std::byte* end = ptr + data.size();

    while (ptr < end) {
        uint32_t ts, sn, una, len, segment_conv;
        uint16_t wnd;
        uint8_t cmd, frg;

        if (std::distance(ptr, end) < IKCP_OVERHEAD) {
            break;
        }

        ptr = encoder::decode32u(ptr, segment_conv);
        ptr = encoder::decode8u(ptr, cmd);
        ptr = encoder::decode8u(ptr, frg);
        ptr = encoder::decode16u(ptr, wnd);
        ptr = encoder::decode32u(ptr, ts);
        ptr = encoder::decode32u(ptr, sn);
        ptr = encoder::decode32u(ptr, una);
        ptr = encoder::decode32u(ptr, len);

        if (segment_conv != conv) {
            return -1;
        }

        if (std::distance(ptr, end) < static_cast<long>(len)) {
            return -2;
        }

        switch (cmd) {
            case IKCP_CMD_ACK: {
                this->parse_ack(sn);
                this->shrink_buf();

                if (flag == 0) {
                    flag = 1;
                    maxack = sn;
                    latest_ts = ts;
                }
                else {
                    if (_itimediff(sn, maxack) > 0) {
    #ifndef IKCP_FASTACK_CONSERVE
                        maxack = sn;
                        latest_ts = ts;
    #else
                        if (_itimediff(ts, latest_ts) > 0) {
                            maxack = sn;
                            latest_ts = ts;
                        }
    #endif
                    }
                }

                break;
            }
            case IKCP_CMD_PUSH: {
                if (_itimediff(sn, this->rcv_nxt + this->rcv_wnd) < 0) {
                    this->ack_push(sn, ts);
                    if (_itimediff(sn, this->rcv_nxt) >= 0) {
                        segment seg = segment(len);
                        seg.conv = conv;
                        seg.cmd = cmd;
                        seg.frg = frg;
                        seg.wnd = wnd;
                        seg.ts = ts;
                        seg.sn = sn;
                        seg.una = una;
                        seg.len = len;

                        if (len > 0) {
                            std::memcpy(seg.data.data(), data.data(), len);
                        }

                        this->parse_data(seg);
                    }
                }
                break;
            }
            case IKCP_CMD_WASK: {
                this->probe |= IKCP_ASK_TELL;
                break;
            }
            case IKCP_CMD_WINS: {
                // Do nothing
                break;
            }
            default: {
                return -3;
            }
        }

        ptr += len;
    }

    if (flag != 0) {
        this->parse_fastack(maxack, latest_ts);
    }

    if (_itimediff(this->snd_una, prev_una) > 0) {
        if (this->cwnd < this->rmt_wnd) {
            i32 mss = this->mss;
            if (this->cwnd < this->ssthresh) {
                this->cwnd++;
                this->incr += mss;
            }
            else {
                if (this->incr < mss) this->incr = mss;
                this->incr += (mss * mss) / this->incr + (mss / 16);
                if ((this->cwnd + 1) * mss <= this->incr) {
                    this->cwnd = (this->incr + mss - 1) / ((mss > 0) ? mss : 1);
                }
            }
            if (this->cwnd > this->rmt_wnd) {
                this->cwnd = this->rmt_wnd;
                this->incr = this->rmt_wnd * mss;
            }
        }
    }

    return 0; // Success
}

void imkcpp::update(u32 current) {
    this->current = current;

    if (this->updated == 0) {
        this->updated = 1;
        this->ts_flush = this->current;
    }

    i32 slap = _itimediff(this->current, this->ts_flush);

    if (slap >= 10000 || slap < -10000) {
        this->ts_flush = this->current;
        slap = 0;
    }

    if (slap >= 0) {
        this->ts_flush += this->interval;
        if (_itimediff(this->current, this->ts_flush) >= 0) {
            this->ts_flush = this->current + this->interval;
        }
        this->flush();
    }
}

u32 imkcpp::check(u32 current) {
    i32 tm_flush = 0x7fffffff;
    i32 tm_packet = 0x7fffffff;
    u32 minimal = 0;

    if (updated == 0) {
        return current;
    }

    if (_itimediff(current, ts_flush) >= 10000 || _itimediff(current, ts_flush) < -10000) {
        ts_flush = current;
    }

    if (_itimediff(current, ts_flush) >= 0) {
        return current;
    }

    tm_flush = _itimediff(ts_flush, current);

    for (const auto& seg : snd_buf) {
        i32 diff = _itimediff(seg.resendts, current);

        if (diff <= 0) {
            return current;
        }

        if (diff < tm_packet) {
            tm_packet = diff;
        }
    }

    minimal = static_cast<u32>(tm_packet < tm_flush ? tm_packet : tm_flush);

    if (minimal >= interval) {
        minimal = interval;
    }

    return current + minimal;
}
