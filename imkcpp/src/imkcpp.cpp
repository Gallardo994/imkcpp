#include <cstring>
#include <cassert>

#include "imkcpp.hpp"
#include "constants.hpp"
#include "commands.hpp"

namespace imkcpp {
    static i32 _itimediff(u32 later, u32 earlier) {
        return static_cast<i32>(later - earlier);
    }

    ImKcpp::ImKcpp(const u32 conv) : conv(conv) {
        this->set_mtu(constants::IKCP_MTU_DEF);
    }

    void ImKcpp::set_userdata(void* userdata) {
        this->user = userdata;
    }


    void ImKcpp::set_output(const output_callback_t& output) {
        this->output = output;
    }

    void ImKcpp::set_interval(const u32 interval) {
        this->interval = interval;
    }

    void ImKcpp::set_nodelay(const i32 nodelay, u32 interval, const i32 resend, const bool congestion_window) {
        if (nodelay >= 0) {
            this->nodelay = nodelay;

            if (nodelay) {
                this->rx_minrto = constants::IKCP_RTO_NDL;
            } else {
                this->rx_minrto = constants::IKCP_RTO_MIN;
            }
        }

        this->interval = std::clamp(interval, static_cast<u32>(10), static_cast<u32>(5000));

        if (resend >= 0) {
            this->fastresend = resend;
        }

        this->congestion_window = congestion_window;
    }

    tl::expected<size_t, error> ImKcpp::set_mtu(const u32 mtu) {
        if (mtu <= constants::IKCP_OVERHEAD) {
            return tl::unexpected(error::less_than_header_size);
        }

        // TODO: Does this really need triple the size?
        this->buffer.resize(static_cast<size_t>(mtu + constants::IKCP_OVERHEAD) * 3);
        this->mtu = mtu;
        this->mss = this->mtu - constants::IKCP_OVERHEAD;

        return mtu;
    }

    u32 ImKcpp::get_mtu() const {
        return this->mtu;
    }

    u32 ImKcpp::get_max_segment_size() const {
        return this->mss;
    }

    void ImKcpp::set_wndsize(const u32 sndwnd, const u32 rcvwnd) {
        if (sndwnd > 0) {
            this->snd_wnd = sndwnd;
        }

        if (rcvwnd > 0) {
            this->rcv_wnd = std::max(rcvwnd, constants::IKCP_WND_RCV);
        }
    }

    tl::expected<size_t, error> ImKcpp::peek_size() const {
        if (this->rcv_queue.empty()) {
            return tl::unexpected(error::queue_empty);
        }

        const Segment& front = this->rcv_queue.front();

        if (front.header.frg == 0) {
            return front.data_size();
        }

        if (this->rcv_queue.size() < front.header.frg + 1) {
            return tl::unexpected(error::waiting_for_fragment);
        }

        size_t length = 0;
        for (const Segment& seg : this->rcv_queue) {
            length += seg.data_size();

            if (seg.header.frg == 0) {
                break;
            }
        }

        return length;
    }

    // Parse ack

    void ImKcpp::update_ack(const i32 rtt) {
        if (this->rx_srtt == 0) {
            this->rx_srtt = rtt;
            this->rx_rttval = rtt / 2;
        } else {
            i32 delta = rtt - static_cast<i32>(this->rx_srtt);
            delta = delta >= 0 ? delta : -delta;

            this->rx_rttval = (3 * this->rx_rttval + delta) / 4;
            this->rx_srtt = (7 * this->rx_srtt + rtt) / 8;

            if (this->rx_srtt < 1) {
                this->rx_srtt = 1;
            }
        }

        const u32 rto = this->rx_srtt + std::max(this->interval, 4 * this->rx_rttval);
        this->rx_rto = std::clamp(rto, this->rx_minrto, constants::IKCP_RTO_MAX);
    }

    void ImKcpp::shrink_buf() {
        if (!this->snd_buf.empty()) {
            const Segment& seg = this->snd_buf.front();
            this->snd_una = seg.header.sn;
        } else {
            this->snd_una = this->snd_nxt;
        }
    }

    void ImKcpp::parse_ack(const u32 sn) {
        if (_itimediff(sn, this->snd_una) < 0 || _itimediff(sn, this->snd_nxt) >= 0) {
            return;
        }

        for (auto it = this->snd_buf.begin(); it != this->snd_buf.end();) {
            if (sn == it->header.sn) {
                this->snd_buf.erase(it);
                break;
            }

            if (_itimediff(sn, it->header.sn) < 0) {
                break;
            }

            ++it;
        }
    }

    void ImKcpp::parse_una(const u32 una) {
        for (auto it = this->snd_buf.begin(); it != this->snd_buf.end();) {
            if (_itimediff(una, it->header.sn) > 0) {
                it = this->snd_buf.erase(it);
            } else {
                break;
            }
        }
    }

    void ImKcpp::parse_fastack(const u32 sn, const u32 ts) {
        if (_itimediff(sn, this->snd_una) < 0 || _itimediff(sn, this->snd_nxt) >= 0) {
            return;
        }

        for (Segment& seg : this->snd_buf) {
            if (_itimediff(sn, seg.header.sn) < 0) {
                break;
            }

            if (sn != seg.header.sn) {
#ifndef IKCP_FASTACK_CONSERVE
                seg.metadata.fastack++;
#else
                if (_itimediff(ts, seg.ts) >= 0) {
                    seg.metadata.fastack++;
                }
#endif
            }
        }
    }

    // ack append
    std::optional<Ack> ImKcpp::ack_get(const size_t p) const {
        if (p >= this->acklist.size()) {
            return std::nullopt;
        }

        return this->acklist.at(p);
    }

    // receive

    tl::expected<size_t, error> ImKcpp::recv(std::span<std::byte> buffer) {
        const auto peeksize = this->peek_size();

        if (!peeksize.has_value()) {
            return tl::unexpected(peeksize.error());
        }

        if (peeksize.value() > buffer.size()) {
            return tl::unexpected(error::buffer_too_small);
        }

        const bool recover = this->rcv_queue.size() >= this->rcv_wnd;

        size_t offset = 0;
        for (auto it = this->rcv_queue.begin(); it != this->rcv_queue.end();) {
            Segment& segment = *it;
            const u8 fragment = segment.header.frg;

            const size_t copy_len = std::min(segment.data_size(), buffer.size() - offset);
            segment.data.encode_to(buffer, offset, copy_len);

            it = this->rcv_queue.erase(it);

            if (fragment == 0) {
                break;
            }

            if (offset >= peeksize.value()) {
                break;
            }
        }

        assert(offset == peeksize);

        while (!this->rcv_buf.empty()) {
            Segment& seg = this->rcv_buf.front();
            if (seg.header.sn != this->rcv_nxt || this->rcv_queue.size() >= this->rcv_wnd) {
                break;
            }

            this->rcv_queue.push_back(std::move(seg));
            this->rcv_buf.pop_front();
            this->rcv_nxt++;
        }

        if (this->rcv_queue.size() < this->rcv_wnd && recover) {
            this->probe |= constants::IKCP_ASK_TELL;
        }

        return offset;
    }

    size_t ImKcpp::estimate_segments_count(const size_t size) const {
        return std::max(static_cast<size_t>(1), (size + this->mss - 1) / this->mss);
    }

    tl::expected<size_t, error> ImKcpp::send(std::span<const std::byte> buffer) {
        if (buffer.empty()) {
            return tl::unexpected(error::buffer_too_small);
        }

        const size_t count = estimate_segments_count(buffer.size());

        if (count > std::numeric_limits<u8>::max()) {
            return tl::unexpected(error::too_many_fragments);
        }

        if (count > this->rcv_wnd) {
            return tl::unexpected(error::exceeds_window_size);
        }

        size_t offset = 0;

        for (size_t i = 0; i < count; i++) {
            const size_t size = std::min(buffer.size() - offset, this->mss);
            assert(size > 0);

            Segment& seg = snd_queue.emplace_back();
            seg.data_assign({buffer.data() + offset, size});
            seg.header.frg = static_cast<u8>(count - i - 1);

            assert(seg.data_size() == size);

            offset += size;
        }

        return offset;
    }

    void ImKcpp::parse_data(const Segment& newseg) {
        u32 sn = newseg.header.sn;
        bool repeat = false;

        if (_itimediff(sn, rcv_nxt + rcv_wnd) >= 0 || _itimediff(sn, rcv_nxt) < 0) {
            return;
        }

        const auto it = std::find_if(rcv_buf.rbegin(), rcv_buf.rend(), [sn](const Segment& seg) {
            return _itimediff(sn, seg.header.sn) <= 0;
        });

        if (it != rcv_buf.rend() && it->header.sn == sn) {
            repeat = true;
        }

        if (!repeat) {
            rcv_buf.insert(it.base(), newseg);
        }

        // Move available data from rcv_buf to rcv_queue
        while (!rcv_buf.empty()) {
            Segment& seg = rcv_buf.front();

            if (seg.header.sn == rcv_nxt && this->rcv_queue.size() < static_cast<size_t>(rcv_wnd)) {
                rcv_queue.push_back(std::move(seg));
                rcv_buf.pop_front();
                rcv_nxt++;
            } else {
                break;
            }
        }
    }

    tl::expected<size_t, error> ImKcpp::input(std::span<const std::byte> data) {
        if (data.size() < constants::IKCP_OVERHEAD) {
            return tl::unexpected(error::less_than_header_size);
        }

        if (data.size() > this->mtu) {
            return tl::unexpected(error::more_than_mtu);
        }

        const u32 prev_una = this->snd_una;
        u32 maxack = 0, latest_ts = 0;
        bool flag = false;

        size_t offset = 0;

        while (offset + constants::IKCP_OVERHEAD < data.size()) {
            SegmentHeader header;
            header.decode_from(data, offset);

            if (header.conv != this->conv) {
                return tl::unexpected(error::conv_mismatch);
            }

            if (header.len > data.size() - offset) {
                return tl::unexpected(error::header_and_payload_length_mismatch);
            }

            if (!commands::is_valid(header.cmd)) {
                return tl::unexpected(error::unknown_command);
            }

            this->rmt_wnd = header.wnd;
            this->parse_una(header.una);
            this->shrink_buf();

            switch (header.cmd) {
                case commands::IKCP_CMD_ACK: {
                    if (_itimediff(this->current, header.ts) >= 0) {
                        this->update_ack(_itimediff(this->current, header.ts));
                    }

                    this->parse_ack(header.sn);
                    this->shrink_buf();

                    if (!flag) {
                        flag = true;
                        maxack = header.sn;
                        latest_ts = header.ts;
                    } else {
                        if (_itimediff(header.sn, maxack) > 0) {
    #ifndef IKCP_FASTACK_CONSERVE
                            maxack = header.sn;
                            latest_ts = header.ts;
    #else
                            if (_itimediff(header.ts, latest_ts) > 0) {
                                maxack = sn;
                                latest_ts = ts;
                            }
    #endif
                        }
                    }

                    break;
                }
                case commands::IKCP_CMD_PUSH: {
                    if (_itimediff(header.sn, this->rcv_nxt + this->rcv_wnd) < 0) {
                        this->acklist.emplace_back(header.sn, header.ts);
                        if (_itimediff(header.sn, this->rcv_nxt) >= 0) {
                            Segment seg;
                            seg.header = header; // TODO: Remove this copy
                            seg.data.decode_from(data, offset, header.len);

                            this->parse_data(seg);
                        }
                    }
                    break;
                }
                case commands::IKCP_CMD_WASK: {
                    this->probe |= constants::IKCP_ASK_TELL;
                    break;
                }
                case commands::IKCP_CMD_WINS: {
                    // Do nothing
                    break;
                }
                default: {
                    return tl::unexpected(error::unknown_command);
                }
            }
        }

        if (flag) {
            this->parse_fastack(maxack, latest_ts);
        }

        if (_itimediff(this->snd_una, prev_una) > 0) {
            if (this->cwnd < this->rmt_wnd) {
                const u32 mss = this->mss;

                if (this->cwnd < this->ssthresh) {
                    this->cwnd++;
                    this->incr += mss;
                } else {
                    if (this->incr < mss) {
                        this->incr = mss;
                    }

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

        return offset;
    }

    FlushResult ImKcpp::update(u32 current) {
        this->current = current;

        if (!this->updated) {
            this->updated = true;
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
            return this->flush();
        }

        return {};
    }

    // TODO: Implement
    u32 ImKcpp::flush_acks() {
        return 0;
    }


    FlushResult ImKcpp::flush() {
        if (!this->updated) {
            return {};
        }

        const u32 current = this->current;
        int change = 0;
        bool lost = false;

        FlushResult result;
        size_t offset = 0;
        size_t flushed_total_size = 0;

        // TODO: Remake to a standalone buffer class with auto-flush on overflow
        auto flush_buffer = [this, &offset, &flushed_total_size] {
            flushed_total_size += offset;

            if (this->output) {
                this->output({this->buffer.data(), offset}, *this, this->user);
            }

            offset = 0;
        };

        const i32 wnd = this->wnd_unused();

        {
            Segment seg;
            seg.header.conv = this->conv;
            seg.header.cmd = commands::IKCP_CMD_ACK;
            seg.header.frg = 0;
            seg.header.wnd = wnd;
            seg.header.una = this->rcv_nxt;
            seg.header.sn = 0;
            seg.header.ts = 0;
            seg.header.len = 0;

            // flush acknowledges
            for (const Ack& ack : this->acklist) {
                if (offset > this->mss) {
                    flush_buffer();
                }

                seg.header.sn = ack.sn;
                seg.header.ts = ack.ts;

                seg.encode_to(this->buffer, offset);
            }

            result.ack_sent_count += this->acklist.size();
            this->acklist.clear();

            // probe window size (if remote window size equals zero)
            if (this->rmt_wnd == 0) {
                if (this->probe_wait == 0) {
                    this->probe_wait = constants::IKCP_PROBE_INIT;
                    this->ts_probe = this->current + this->probe_wait;
                } else {
                    if (_itimediff(this->current, this->ts_probe) >= 0) {
                        if (this->probe_wait < constants::IKCP_PROBE_INIT) {
                            this->probe_wait = constants::IKCP_PROBE_INIT;
                        }

                        this->probe_wait += this->probe_wait / 2;
                        if (this->probe_wait > constants::IKCP_PROBE_LIMIT) {
                            this->probe_wait = constants::IKCP_PROBE_LIMIT;
                        }
                        this->ts_probe = this->current + this->probe_wait;
                        this->probe |= constants::IKCP_ASK_SEND;
                    }
                }
            } else {
                this->ts_probe = 0;
                this->probe_wait = 0;
            }

            // flush window probing commands
            if (this->probe & constants::IKCP_ASK_SEND) {
                if (offset > this->mss) {
                    flush_buffer();
                }

                seg.header.cmd = commands::IKCP_CMD_WASK;
                seg.encode_to(this->buffer, offset);

                result.cmd_wask_count++;
            }

            // flush window probing commands
            if (this->probe & constants::IKCP_ASK_TELL) {
                if (offset > this->mss) {
                    flush_buffer();
                }

                seg.header.cmd = commands::IKCP_CMD_WINS;
                seg.encode_to(this->buffer, offset);

                result.cmd_wins_count++;
            }

            this->probe = 0;
        }

        // calculate window size
        u32 cwnd = std::min(this->snd_wnd, this->rmt_wnd);
        if (this->congestion_window) {
            cwnd = std::min(this->cwnd, cwnd);
        }

        // move data from snd_queue to snd_buf
        while (_itimediff(this->snd_nxt, this->snd_una + cwnd) < 0) {
            if (snd_queue.empty()) {
                break;
            }

            Segment& newseg = snd_queue.front();

            newseg.header.conv = this->conv;
            newseg.header.cmd = commands::IKCP_CMD_PUSH;
            newseg.header.wnd = wnd;
            newseg.header.ts = current;
            newseg.header.sn = this->snd_nxt++;
            newseg.header.una = this->rcv_nxt;
            newseg.metadata.resendts = current;
            newseg.metadata.rto = this->rx_rto;
            newseg.metadata.fastack = 0;
            newseg.metadata.xmit = 0;

            snd_buf.push_back(std::move(newseg));
            snd_queue.pop_front();
        }

        // calculate resent
        u32 resent = (this->fastresend > 0) ? this->fastresend : 0xffffffff;
        u32 rtomin = (this->nodelay == 0) ? (this->rx_rto >> 3) : 0;

        // flush data segments
        for (Segment& segment : this->snd_buf) {
            bool needsend = false;

            if (segment.metadata.xmit == 0) {
                needsend = true;
                segment.metadata.xmit++;
                segment.metadata.rto = this->rx_rto;
                segment.metadata.resendts = current + segment.metadata.rto + rtomin;
            } else if (_itimediff(current, segment.metadata.resendts) >= 0) {
                needsend = true;
                segment.metadata.xmit++;
                this->xmit++;

                if (this->nodelay == 0) {
                    segment.metadata.rto += std::max(segment.metadata.rto, this->rx_rto);
                } else {
                    const u32 step = this->nodelay < 2? segment.metadata.rto : this->rx_rto;
                    segment.metadata.rto += step / 2;
                }

                segment.metadata.resendts = current + segment.metadata.rto;
                lost = true;
            } else if (segment.metadata.fastack >= resent) {
                // TODO: The second check is probably redundant
                if (segment.metadata.xmit <= this->fastlimit || this->fastlimit <= 0) {
                    needsend = true;
                    segment.metadata.xmit++;
                    segment.metadata.fastack = 0;
                    segment.metadata.resendts = current + segment.metadata.rto;
                    change++;
                }
            }

            if (needsend) {
                segment.header.ts = current;
                segment.header.wnd = wnd;
                segment.header.una = this->rcv_nxt;

                if (offset + segment.data_size() > this->mss) {
                    flush_buffer();
                }

                segment.encode_to(this->buffer, offset);

                if (segment.metadata.xmit >= this->dead_link) {
                    this->state = State::DeadLink;
                }

                result.data_sent_count++;
                result.retransmitted_count += segment.metadata.xmit > 1 ? 1 : 0;
            }
        }

        // TODO: Remake to a standalone buffer class with auto-flush on overflow,
        // TODO: don't forget to add flush_if_not_empty() method here
        // flash remain segments
        if (offset > 0) {
            flush_buffer();
        }

        // update ssthresh
        if (change) {
            const u32 inflight = this->snd_nxt - this->snd_una;
            this->ssthresh = std::max(inflight / 2, constants::IKCP_THRESH_MIN);
            this->cwnd = this->ssthresh + resent;
            this->incr = this->cwnd * this->mss;
        }

        if (lost) {
            this->ssthresh = std::max(cwnd / 2, constants::IKCP_THRESH_MIN);
            this->cwnd = 1;
            this->incr = this->mss;
        }

        if (this->cwnd < 1) {
            this->cwnd = 1;
            this->incr = this->mss;
        }

        result.total_bytes_sent = flushed_total_size;
        return result;
    }


    u32 ImKcpp::check(const u32 current) {
        i32 tm_flush = 0x7fffffff;
        i32 tm_packet = 0x7fffffff;
        u32 minimal = 0;

        if (!this->updated) {
            return current;
        }

        if (_itimediff(current, this->ts_flush) >= 10000 || _itimediff(current, this->ts_flush) < -10000) {
            this->ts_flush = current;
        }

        if (_itimediff(current, this->ts_flush) >= 0) {
            return current;
        }

        tm_flush = _itimediff(this->ts_flush, current);

        for (const Segment& seg : this->snd_buf) {
            const i32 diff = _itimediff(seg.metadata.resendts, current);

            if (diff <= 0) {
                return current;
            }

            if (diff < tm_packet) {
                tm_packet = diff;
            }
        }

        minimal = static_cast<u32>(tm_packet < tm_flush ? tm_packet : tm_flush);

        if (minimal >= this->interval) {
            minimal = this->interval;
        }

        return current + minimal;
    }

    i32 ImKcpp::wnd_unused() const {
        if (this->rcv_queue.size() < this->rcv_wnd) {
            return static_cast<i32>(this->rcv_wnd - this->rcv_queue.size());
        }

        return 0;
    }
}