#pragma once

namespace imkcpp {
    struct FlushResult {
        u32 ack_sent_count = 0; // Number of ACKs sent
        u32 cmd_wask_count = 0; // Number of WASK commands sent
        u32 cmd_wins_count = 0; // Number of WINS commands sent

        u32 data_sent_count = 0; // Number of data segments sent
        u32 retransmitted_count = 0; // Number of retransmitted segments
        size_t total_bytes_sent = 0; // Total number of bytes sent

        FlushResult operator+(const FlushResult& other) const {
            return {
                ack_sent_count + other.ack_sent_count,
                cmd_wask_count + other.cmd_wask_count,
                cmd_wins_count + other.cmd_wins_count,
                data_sent_count + other.data_sent_count,
                retransmitted_count + other.retransmitted_count,
                total_bytes_sent + other.total_bytes_sent
            };
        }

        FlushResult& operator+=(const FlushResult& other) {
            ack_sent_count += other.ack_sent_count;
            cmd_wask_count += other.cmd_wask_count;
            cmd_wins_count += other.cmd_wins_count;
            data_sent_count += other.data_sent_count;
            retransmitted_count += other.retransmitted_count;
            total_bytes_sent += other.total_bytes_sent;

            return *this;
        }
    };
}