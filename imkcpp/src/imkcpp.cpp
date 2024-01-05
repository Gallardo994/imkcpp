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