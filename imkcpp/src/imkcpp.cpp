#include <cstring>
#include <cassert>

#include "imkcpp.hpp"
#include "constants.hpp"
#include "commands.hpp"
#include "utility.hpp"

// TODO: > 600 lines of code, gg. Split into different implementation files.
namespace imkcpp {
    ImKcpp::ImKcpp(const u32 conv) : conv(conv) {
        this->set_mtu(constants::IKCP_MTU_DEF);
    }

    void ImKcpp::set_userdata(void* userdata) {
        this->user = userdata;
    }

    State ImKcpp::get_state() const {
        return this->state;
    }

    void ImKcpp::set_output(const output_callback_t& output) {
        this->output = output;
    }

    void ImKcpp::set_interval(const u32 interval) {
        this->interval = std::clamp(interval, static_cast<u32>(10), static_cast<u32>(5000));
    }

    // TODO: Needs to be separate functions, and should be able to set them independently
    // TODO: Also, should return a tl::expected with the new value or an error
    void ImKcpp::set_nodelay(const i32 nodelay, u32 interval, const i32 resend, const bool congestion_window_state) {
        if (nodelay >= 0) {
            this->nodelay = nodelay;
            this->rto_calculator.set_min_rto(nodelay > 0 ? constants::IKCP_RTO_NDL : constants::IKCP_RTO_MIN);
        }

        this->set_interval(interval);

        if (resend >= 0) {
            this->fastresend = resend;
        }

        this->set_congestion_window_enabled(congestion_window_state);
    }

    void ImKcpp::set_congestion_window_enabled(const bool state) {
        this->congestion_controller.set_congestion_window_enabled(state);
    }

    tl::expected<size_t, error> ImKcpp::set_mtu(const u32 mtu) {
        if (mtu <= constants::IKCP_OVERHEAD) {
            return tl::unexpected(error::less_than_header_size);
        }

        // TODO: Does this really need triple the size?
        this->buffer.resize(static_cast<size_t>(mtu + constants::IKCP_OVERHEAD) * 3);
        this->mtu = mtu;
        this->mss = this->mtu - constants::IKCP_OVERHEAD;

        this->congestion_controller.set_mss(this->mss);

        return mtu;
    }

    u32 ImKcpp::get_mtu() const {
        return this->mtu;
    }

    u32 ImKcpp::get_max_segment_size() const {
        return this->mss;
    }

    // TODO: Needs to be separate functions, and should be able to set them independently
    // TODO: Also, should return a tl::expected with the new value or an error
    void ImKcpp::set_wndsize(const u32 sndwnd, const u32 rcvwnd) {
        if (sndwnd > 0) {
            this->congestion_controller.set_send_window(sndwnd);
            this->congestion_controller.set_remote_window(sndwnd);
        }

        if (rcvwnd > 0) {
            this->congestion_controller.set_receive_window(rcvwnd);
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

        if (this->rcv_queue.size() < static_cast<size_t>(front.header.frg) + 1) {
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

    void ImKcpp::shrink_buf() {
        if (!this->snd_buf.empty()) {
            const Segment& seg = this->snd_buf.front();
            this->snd_una = seg.header.sn;
        } else {
            this->snd_una = this->snd_nxt;
        }
    }

    void ImKcpp::parse_ack(const u32 sn) {
        if (time_delta(sn, this->snd_una) < 0 || time_delta(sn, this->snd_nxt) >= 0) {
            return;
        }

        for (auto it = this->snd_buf.begin(); it != this->snd_buf.end();) {
            if (sn == it->header.sn) {
                this->snd_buf.erase(it);
                break;
            }

            if (time_delta(sn, it->header.sn) < 0) {
                break;
            }

            ++it;
        }
    }

    void ImKcpp::parse_una(const u32 una) {
        for (auto it = this->snd_buf.begin(); it != this->snd_buf.end();) {
            if (time_delta(una, it->header.sn) > 0) {
                it = this->snd_buf.erase(it);
            } else {
                break;
            }
        }
    }

    void ImKcpp::parse_fastack(const u32 sn, const u32 ts) {
        if (time_delta(sn, this->snd_una) < 0 || time_delta(sn, this->snd_nxt) >= 0) {
            return;
        }

        for (Segment& seg : this->snd_buf) {
            if (time_delta(sn, seg.header.sn) < 0) {
                break;
            }

            if (sn != seg.header.sn) {
#ifndef IKCP_FASTACK_CONSERVE
                seg.metadata.fastack++;
#else
                if (time_delta(ts, seg.ts) >= 0) {
                    seg.metadata.fastack++;
                }
#endif
            }
        }
    }

    std::optional<Ack> ImKcpp::ack_get(const size_t p) const {
        if (p >= this->acklist.size()) {
            return std::nullopt;
        }

        return this->acklist.at(p);
    }

    tl::expected<size_t, error> ImKcpp::recv(std::span<std::byte> buffer) {
        const auto peeksize = this->peek_size();

        if (!peeksize.has_value()) {
            return tl::unexpected(peeksize.error());
        }

        if (peeksize.value() > buffer.size()) {
            return tl::unexpected(error::buffer_too_small);
        }

        const bool recover = this->rcv_queue.size() >= this->congestion_controller.get_receive_window();

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
            if (seg.header.sn != this->rcv_nxt || this->rcv_queue.size() >= this->congestion_controller.get_receive_window()) {
                break;
            }

            this->rcv_queue.push_back(std::move(seg));
            this->rcv_buf.pop_front();
            this->rcv_nxt++;
        }

        if (this->congestion_controller.get_receive_window() > this->rcv_queue.size() && recover) {
            this->congestion_controller.set_probe_flag(constants::IKCP_ASK_TELL);
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

        if (count > this->congestion_controller.get_send_window()) {
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

        if (time_delta(sn, this->rcv_nxt + this->congestion_controller.get_receive_window()) >= 0 || time_delta(sn, this->rcv_nxt) < 0) {
            return;
        }

        const auto it = std::find_if(this->rcv_buf.rbegin(), this->rcv_buf.rend(), [sn](const Segment& seg) {
            return time_delta(sn, seg.header.sn) <= 0;
        });

        if (it != this->rcv_buf.rend() && it->header.sn == sn) {
            repeat = true;
        }

        if (!repeat) {
            this->rcv_buf.insert(it.base(), newseg);
        }

        // Move available data from rcv_buf to rcv_queue
        while (!this->rcv_buf.empty()) {
            Segment& seg = this->rcv_buf.front();

            if (seg.header.sn == this->rcv_nxt && this->rcv_queue.size() < static_cast<size_t>(this->congestion_controller.get_receive_window())) {
                this->rcv_queue.push_back(std::move(seg));
                this->rcv_buf.pop_front();
                this->rcv_nxt++;
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

        while (offset + constants::IKCP_OVERHEAD <= data.size()) {
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

            this->congestion_controller.set_remote_window(header.wnd);
            this->parse_una(header.una);
            this->shrink_buf();

            switch (header.cmd) {
                case commands::IKCP_CMD_ACK: {
                    if (time_delta(this->current, header.ts) >= 0) {
                        const i32 rtt = time_delta(this->current, header.ts);
                        this->rto_calculator.update_rtt(rtt, this->interval);
                    }

                    this->parse_ack(header.sn);
                    this->shrink_buf();

                    if (!flag) {
                        flag = true;
                        maxack = header.sn;
                        latest_ts = header.ts;
                    } else {
                        if (time_delta(header.sn, maxack) > 0) {
    #ifndef IKCP_FASTACK_CONSERVE
                            maxack = header.sn;
                            latest_ts = header.ts;
    #else
                            if (time_delta(header.ts, latest_ts) > 0) {
                                maxack = sn;
                                latest_ts = ts;
                            }
    #endif
                        }
                    }

                    break;
                }
                case commands::IKCP_CMD_PUSH: {
                    if (time_delta(header.sn, this->rcv_nxt + this->congestion_controller.get_receive_window()) < 0) {
                        this->acklist.emplace_back(header.sn, header.ts);
                        if (time_delta(header.sn, this->rcv_nxt) >= 0) {
                            Segment seg;
                            seg.header = header; // TODO: Remove this copy
                            seg.data.decode_from(data, offset, header.len);

                            this->parse_data(seg);
                        }
                    }
                    break;
                }
                case commands::IKCP_CMD_WASK: {
                    this->congestion_controller.set_probe_flag(constants::IKCP_ASK_TELL);
                    break;
                }
                case commands::IKCP_CMD_WINS: {
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

        congestion_controller.packet_acked(this->snd_una, prev_una);

        return offset;
    }

    FlushResult ImKcpp::update(const u32 current) {
        this->current = current;

        if (!this->updated) {
            this->updated = true;
            this->ts_flush = this->current;
        }

        i32 slap = time_delta(this->current, this->ts_flush);

        if (slap >= 10000 || slap < -10000) {
            this->ts_flush = this->current;
            slap = 0;
        }

        if (slap >= 0) {
            this->ts_flush += this->interval;
            if (time_delta(this->current, this->ts_flush) >= 0) {
                this->ts_flush = this->current + this->interval;
            }
            return this->flush();
        }

        return {};
    }

    FlushResult ImKcpp::flush() {
        FlushResult result;

        if (!this->updated) {
            return result;
        }

        const u32 current = this->current;
        bool change = false;
        bool lost = false;

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

        const i32 unused_receive_window = this->get_unused_receive_window();

        {
            Segment seg;
            seg.header.conv = this->conv;
            seg.header.cmd = commands::IKCP_CMD_ACK;
            seg.header.frg = 0;
            seg.header.wnd = unused_receive_window;
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

            this->congestion_controller.update_probe_request(current);

            if (this->congestion_controller.has_probe_flag(constants::IKCP_ASK_SEND)) {
                if (offset > this->mss) {
                    flush_buffer();
                }

                seg.header.cmd = commands::IKCP_CMD_WASK;
                seg.encode_to(this->buffer, offset);

                result.cmd_wask_count++;
            }

            if (this->congestion_controller.has_probe_flag(constants::IKCP_ASK_TELL)) {
                if (offset > this->mss) {
                    flush_buffer();
                }

                seg.header.cmd = commands::IKCP_CMD_WINS;
                seg.encode_to(this->buffer, offset);

                result.cmd_wins_count++;
            }

            this->congestion_controller.reset_probe_flags();
        }

        const u32 cwnd = this->congestion_controller.calculate_congestion_window();

        while (!snd_queue.empty() && this->snd_nxt < this->snd_una + cwnd) {
            Segment& newseg = snd_queue.front();

            newseg.header.conv = this->conv;
            newseg.header.cmd = commands::IKCP_CMD_PUSH;
            newseg.header.wnd = unused_receive_window;
            newseg.header.ts = current;
            newseg.header.sn = this->snd_nxt++;
            newseg.header.una = this->rcv_nxt;

            newseg.metadata.resendts = current;
            newseg.metadata.rto = this->rto_calculator.get_rto();
            newseg.metadata.fastack = 0;
            newseg.metadata.xmit = 0;

            snd_buf.push_back(std::move(newseg));
            snd_queue.pop_front();
        }

        // calculate resent
        const u32 resent = (this->fastresend > 0) ? this->fastresend : 0xffffffff;
        const u32 rtomin = (this->nodelay == 0) ? (this->rto_calculator.get_rto() >> 3) : 0;

        // flush data segments
        for (Segment& segment : this->snd_buf) {
            bool needsend = false;

            if (segment.metadata.xmit == 0) {
                needsend = true;
                segment.metadata.xmit++;
                segment.metadata.rto = this->rto_calculator.get_rto();
                segment.metadata.resendts = current + segment.metadata.rto + rtomin;
            } else if (time_delta(current, segment.metadata.resendts) >= 0) {
                needsend = true;
                segment.metadata.xmit++;
                this->xmit++;

                if (this->nodelay == 0) {
                    segment.metadata.rto += std::max(segment.metadata.rto, this->rto_calculator.get_rto());
                } else {
                    const u32 step = this->nodelay < 2? segment.metadata.rto : this->rto_calculator.get_rto();
                    segment.metadata.rto += step / 2;
                }

                segment.metadata.resendts = current + segment.metadata.rto;
                lost = true;
            } else if (segment.metadata.fastack >= resent) {
                // TODO: The second check is probably redundant
                if (segment.metadata.xmit <= this->fastlimit || this->fastlimit <= 0) {
                    // TODO: Why isn't this->xmit incremented here?
                    needsend = true;
                    change = true;

                    segment.metadata.xmit++;
                    segment.metadata.fastack = 0;
                    segment.metadata.resendts = current + segment.metadata.rto;
                }
            }

            if (needsend) {
                segment.header.ts = current;
                segment.header.wnd = unused_receive_window;
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

        if (change) {
            this->congestion_controller.resent(this->snd_nxt - this->snd_una, resent);
        }

        if (lost) {
            this->congestion_controller.packet_lost();
        }

        this->congestion_controller.ensure_at_least_one_packet_in_flight();

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

        if (std::abs(time_delta(current, this->ts_flush)) >= 10000) {
            this->ts_flush = current;
        }

        if (time_delta(current, this->ts_flush) >= 0) {
            return current;
        }

        tm_flush = time_delta(this->ts_flush, current);

        for (const Segment& seg : this->snd_buf) {
            const i32 diff = time_delta(seg.metadata.resendts, current);

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

    i32 ImKcpp::get_unused_receive_window() const {
        return std::max(static_cast<i32>(this->congestion_controller.get_receive_window()) - static_cast<i32>(this->rcv_queue.size()), 0);
    }
}