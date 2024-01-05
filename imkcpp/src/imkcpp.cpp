#include "imkcpp.hpp"
#include "constants.hpp"

void imkcpp::set_output(const std::function<int(std::span<const std::byte> data, const imkcpp& imkcpp, std::optional<void*> user)>&) {
    this->output = output;
}

void imkcpp::set_interval(const uint32_t interval) {
    this->interval = interval;
}

void imkcpp::set_nodelay(const int32_t nodelay, uint32_t interval, const int32_t resend, const int32_t nc) {
    if (nodelay >= 0) {
        this->nodelay = nodelay;
        if (nodelay) {
            this->rx_minrto = IKCP_RTO_NDL;
        }
        else {
            this->rx_minrto = IKCP_RTO_MIN;
        }
    }

    if (interval > 5000) {
        interval = 5000;
    } else if (interval < 10) {
        interval = 10;
    }

    this->interval = interval;

    if (resend >= 0) {
        this->fastresend = resend;
    }

    if (nc >= 0) {
        this->nocwnd = nc;
    }
}

void imkcpp::set_mtu(const uint32_t mtu) {
    if (mtu <= IKCP_OVERHEAD) {
        return;
    }

    this->buffer.reserve(static_cast<size_t>(mtu + IKCP_OVERHEAD) * 3);
    this->mtu = mtu;
    this->mss = this->mtu - IKCP_OVERHEAD;
}

void imkcpp::set_wndsize(const uint32_t sndwnd, const uint32_t rcvwnd) {
    if (sndwnd > 0) {
        this->snd_wnd = sndwnd;
    }

    if (rcvwnd > 0) {
        this->rcv_wnd = std::max(rcvwnd, IKCP_WND_RCV);
    }
}

int imkcpp::peek_size() const {
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

void imkcpp::update_ack(const int32_t rtt) {
    if (this->rx_srtt == 0) {
        this->rx_srtt = rtt;
        this->rx_rttval = rtt / 2;
    }
    else {
        int32_t delta = rtt - static_cast<int32_t>(this->rx_srtt);
        delta = delta >= 0 ? delta : -delta;

        this->rx_rttval = (3 * this->rx_rttval + delta) / 4;
        this->rx_srtt = (7 * this->rx_srtt + rtt) / 8;

        if (this->rx_srtt < 1) {
            this->rx_srtt = 1;
        }
    }

    const uint32_t rto = this->rx_srtt + std::max(this->interval, 4 * this->rx_rttval);
    this->rx_rto = std::clamp(rto, this->rx_minrto, IKCP_RTO_MAX);
}

// TODO: ikcp_shrink_buf

// TODO: ikcp_parse_ack

// TODO: ikcp_parse_una

// TODO: ikcp_parse_fastack