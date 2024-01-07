#include <cstring>
#include <cassert>

#include "imkcpp.hpp"
#include "constants.hpp"
#include "encoder.hpp"

static i32 _itimediff(u32 later, u32 earlier) {
    return static_cast<i32>(later - earlier);
}

imkcpp::imkcpp(const u32 conv, std::optional<void*> user) : conv(conv), user(user) {
    this->snd_una = 0;
    this->snd_nxt = 0;
    this->rcv_nxt = 0;
    this->ts_probe = 0;
    this->probe_wait = 0;
    this->snd_wnd = IKCP_WND_SND;
    this->rcv_wnd = IKCP_WND_RCV;
    this->rmt_wnd = IKCP_WND_RCV;
    this->cwnd = 0;
    this->incr = 0;
    this->probe = 0;
    this->mtu = IKCP_MTU_DEF;
    this->mss = this->mtu - IKCP_OVERHEAD;
    this->stream = 0;

    this->buffer.reserve((this->mtu + IKCP_OVERHEAD) * 3);

    this->state = imkcpp_state::Alive;
    this->rx_srtt = 0;
    this->rx_rttval = 0;
    this->rx_rto = IKCP_RTO_DEF;
    this->rx_minrto = IKCP_RTO_MIN;
    this->current = 0;
    this->interval = IKCP_INTERVAL;
    this->ts_flush = IKCP_INTERVAL;
    this->nodelay = 0;
    this->updated = 0;
    this->ssthresh = IKCP_THRESH_INIT;
    this->fastresend = 0;
    this->fastlimit = IKCP_FASTACK_LIMIT;
    this->nocwnd = 0;
    this->xmit = 0;
    this->dead_link = IKCP_DEADLINK;
}

void imkcpp::set_output(const std::function<i32(std::span<const std::byte> data, const imkcpp& imkcpp, std::optional<void*> user)>& output) {
    this->output = output;
}

void imkcpp::call_output(const std::span<const std::byte>& data) const {
    if (this->output) {
        this->output(data, *this, this->user);
    }
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

    this->interval = std::clamp(interval, static_cast<u32>(10), static_cast<u32>(5000));

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

u32 imkcpp::get_mtu() const {
    return this->mtu;
}

u32 imkcpp::get_max_segment_size() const {
    return this->mss;
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

    const bool recover = this->rcv_queue.size() >= rcv_wnd;

    size_t len = 0;
    for (auto it = rcv_queue.begin(); it != rcv_queue.end();) {
        const segment& segment = *it;
        const size_t copy_len = std::min(segment.data.size(), buffer.size() - len);
        std::memcpy(buffer.data() + len, segment.data.data(), copy_len);
        len += copy_len;

        it = rcv_queue.erase(it);

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
        if (seg.sn != rcv_nxt || this->rcv_queue.size() >= rcv_wnd) {
            break;
        }

        rcv_queue.push_back(std::move(seg));
        rcv_buf.pop_front();
        rcv_nxt++;
    }

    if (this->rcv_queue.size() < rcv_wnd && recover) {
        this->probe |= IKCP_ASK_TELL;
    }

    return static_cast<i32>(len);
}

// send

i32 imkcpp::send(const std::span<const std::byte>& buffer) {
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

    if (count >= this->rcv_wnd) {
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
    }

    // Move available data from rcv_buf to rcv_queue
    while (!rcv_buf.empty()) {
        segment &seg = rcv_buf.front();
        if (seg.sn == rcv_nxt && this->rcv_queue.size() < static_cast<size_t>(rcv_wnd)) {
            rcv_buf.pop_front();
            rcv_queue.push_back(seg);
            rcv_nxt++;
        } else {
            break;
        }
    }
}

void imkcpp::encode_seg(const segment& seg, std::vector<std::byte>& vector) {
    assert(vector.size() >= IKCP_OVERHEAD);

    encoder::encode32u(vector, seg.conv);
    encoder::encode8u(vector, seg.cmd);
    encoder::encode8u(vector, seg.frg);
    encoder::encode16u(vector, seg.wnd);
    encoder::encode32u(vector, seg.ts);
    encoder::encode32u(vector, seg.sn);
    encoder::encode32u(vector, seg.una);
    encoder::encode32u(vector, seg.len);
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
                        segment seg(len);
                        seg.conv = conv;
                        seg.cmd = cmd;
                        seg.frg = frg;
                        seg.wnd = wnd;
                        seg.ts = ts;
                        seg.sn = sn;
                        seg.una = una;
                        seg.len = len;

                        if (len > 0) {
                            seg.data.insert(seg.data.end(), ptr, ptr + len);
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
            const u32 mss = this->mss;
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

void imkcpp::flush() {
	if (this->updated == 0) {
	    return;
	}

    u32 current = this->current;
    int change = 0;
    int lost = 0;

    segment seg;

	seg.conv = this->conv;
	seg.cmd = IKCP_CMD_ACK;
	seg.frg = 0;
	seg.wnd = this->wnd_unused();
	seg.una = this->rcv_nxt;
	seg.len = 0;
	seg.sn = 0;
	seg.ts = 0;

	// flush acknowledges
    for (const Ack& ack : this->acklist) {
        if (this->buffer.size() > this->mss) {
            this->call_output(this->buffer);
            this->buffer.clear();
        }

        seg.sn = ack.sn;
        seg.ts = ack.ts;

        this->encode_seg(seg, this->buffer);
    }

    this->acklist.clear();

	// probe window size (if remote window size equals zero)
	if (this->rmt_wnd == 0) {
		if (this->probe_wait == 0) {
			this->probe_wait = IKCP_PROBE_INIT;
			this->ts_probe = this->current + this->probe_wait;
		} else {
			if (_itimediff(this->current, this->ts_probe) >= 0) {
				if (this->probe_wait < IKCP_PROBE_INIT) {
				    this->probe_wait = IKCP_PROBE_INIT;
				}

				this->probe_wait += this->probe_wait / 2;
				if (this->probe_wait > IKCP_PROBE_LIMIT) {
				    this->probe_wait = IKCP_PROBE_LIMIT;
				}
				this->ts_probe = this->current + this->probe_wait;
				this->probe |= IKCP_ASK_SEND;
			}
		}
	} else {
		this->ts_probe = 0;
		this->probe_wait = 0;
	}

	// flush window probing commands
	if (this->probe & IKCP_ASK_SEND) {
		if (this->buffer.size() + IKCP_OVERHEAD > this->mss) {
		    this->call_output(this->buffer);
		    this->buffer.clear();
		}

	    seg.cmd = IKCP_CMD_WASK;
	    this->encode_seg(seg, this->buffer);
	}

	// flush window probing commands
	if (this->probe & IKCP_ASK_TELL) {
		if (this->buffer.size() + IKCP_OVERHEAD > this->mss) {
		    this->call_output(this->buffer);
		    this->buffer.clear();
		}

	    seg.cmd = IKCP_CMD_WINS;
	    this->encode_seg(seg, this->buffer);
	}

	this->probe = 0;

	// calculate window size
	u32 cwnd = std::min(this->snd_wnd, this->rmt_wnd);
	if (this->nocwnd == 0) {
	    cwnd = std::min(this->cwnd, cwnd);
	}

	// move data from snd_queue to snd_buf
	while (_itimediff(this->snd_nxt, this->snd_una + cwnd) < 0) {
	    if (snd_queue.empty()) {
	        break;
	    }

	    segment& newseg = snd_queue.front();

		newseg.conv = this->conv;
		newseg.cmd = IKCP_CMD_PUSH;
		newseg.wnd = seg.wnd;
		newseg.ts = current;
		newseg.sn = this->snd_nxt++;
		newseg.una = this->rcv_nxt;
		newseg.resendts = current;
		newseg.rto = this->rx_rto;
		newseg.fastack = 0;
		newseg.xmit = 0;

	    snd_buf.push_back(std::move(newseg));
	    snd_queue.pop_front();
	}

	// calculate resent
	u32 resent = (this->fastresend > 0) ? this->fastresend : 0xffffffff;
	u32 rtomin = (this->nodelay == 0) ? (this->rx_rto >> 3) : 0;

	// flush data segments
    for (segment& segment : this->snd_buf) {
		int needsend = 0;
		if (segment.xmit == 0) {
			needsend = 1;
			segment.xmit++;
			segment.rto = this->rx_rto;
			segment.resendts = current + segment.rto + rtomin;
		}
		else if (_itimediff(current, segment.resendts) >= 0) {
			needsend = 1;
			segment.xmit++;
			this->xmit++;
			if (this->nodelay == 0) {
				segment.rto += std::max(segment.rto, this->rx_rto);
			} else {
				i32 step = (this->nodelay < 2)? static_cast<i32>(segment.rto) : this->rx_rto;
				segment.rto += step / 2;
			}
			segment.resendts = current + segment.rto;
			lost = 1;
		}
		else if (segment.fastack >= resent) {
			if ((int)segment.xmit <= this->fastlimit ||
				this->fastlimit <= 0) {
				needsend = 1;
				segment.xmit++;
				segment.fastack = 0;
				segment.resendts = current + segment.rto;
				change++;
			}
		}

		if (needsend) {
			segment.ts = current;
			segment.wnd = seg.wnd;
			segment.una = this->rcv_nxt;

			if (this->buffer.size() + segment.len > this->mss) {
				call_output(this->buffer);
			    this->buffer.clear();
			}

			this->encode_seg(segment, this->buffer);

			if (segment.len > 0) {
			    encoder::encode_raw(this->buffer, segment.data);
			}

			if (segment.xmit >= this->dead_link) {
				this->state = imkcpp_state::Dead;
			}
		}
	}

	// flash remain segments
	if (!this->buffer.empty()) {
		this->call_output(this->buffer);
	    this->buffer.clear();
	}

	// update ssthresh
	if (change) {
		u32 inflight = this->snd_nxt - this->snd_una;
		this->ssthresh = std::max(inflight / 2, IKCP_THRESH_MIN);
		this->cwnd = this->ssthresh + resent;
		this->incr = this->cwnd * this->mss;
	}

	if (lost) {
		this->ssthresh = std::max(cwnd / 2, IKCP_THRESH_MIN);
		this->cwnd = 1;
		this->incr = this->mss;
	}

	if (this->cwnd < 1) {
		this->cwnd = 1;
		this->incr = this->mss;
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
        const i32 diff = _itimediff(seg.resendts, current);

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

i32 imkcpp::wnd_unused() const {
    if (this->rcv_queue.size() < this->rcv_wnd) {
        return static_cast<i32>(this->rcv_wnd - this->rcv_queue.size());
    }

    return 0;
}