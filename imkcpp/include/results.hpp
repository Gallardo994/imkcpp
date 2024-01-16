#pragma once

namespace imkcpp {
    /// Result of calling input() method.
    struct InputResult final {
        /// Number of ACK commands received
        u32 cmd_ack_count = 0;

        /// Number of WASK commands received
        u32 cmd_wask_count = 0;

        /// Number of WINS commands received
        u32 cmd_wins_count = 0;

        /// Number of PUSH segments received
        u32 cmd_push_count = 0;

        /// Number of PUSH segments dropped
        u32 dropped_push_count = 0;

        /// Total number of bytes received
        size_t total_bytes_received = 0;

        InputResult operator+(const InputResult& other) const {
            return {
                cmd_ack_count + other.cmd_ack_count,
                cmd_wask_count + other.cmd_wask_count,
                cmd_wins_count + other.cmd_wins_count,
                cmd_push_count + other.cmd_push_count,
                dropped_push_count + other.dropped_push_count,
                total_bytes_received + other.total_bytes_received
            };
        }

        InputResult& operator+=(const InputResult& other) {
            cmd_ack_count += other.cmd_ack_count;
            cmd_wask_count += other.cmd_wask_count;
            cmd_wins_count += other.cmd_wins_count;
            cmd_push_count += other.cmd_push_count;
            dropped_push_count += other.dropped_push_count;
            total_bytes_received += other.total_bytes_received;

            return *this;
        }
    };

    /// Result of calling flush() or update() method.
    struct FlushResult final {
        /// Number of ACK commands sent
        u32 cmd_ack_count = 0;

        /// Number of WASK commands sent
        u32 cmd_wask_count = 0;

        /// Number of WINS commands sent
        u32 cmd_wins_count = 0;

        /// Number of PUSH segments sent
        u32 cmd_push_count = 0;

        /// Number of retransmitted segments
        u32 timeout_retransmitted_count = 0;

        /// Number of fast retransmitted segments
        u32 fast_retransmitted_count = 0;

        /// Total number of bytes sent
        size_t total_bytes_sent = 0;

        FlushResult operator+(const FlushResult& other) const {
            return {
                cmd_ack_count + other.cmd_ack_count,
                cmd_wask_count + other.cmd_wask_count,
                cmd_wins_count + other.cmd_wins_count,
                cmd_push_count + other.cmd_push_count,
                timeout_retransmitted_count + other.timeout_retransmitted_count,
                fast_retransmitted_count + other.fast_retransmitted_count,
                total_bytes_sent + other.total_bytes_sent
            };
        }

        FlushResult& operator+=(const FlushResult& other) {
            cmd_ack_count += other.cmd_ack_count;
            cmd_wask_count += other.cmd_wask_count;
            cmd_wins_count += other.cmd_wins_count;
            cmd_push_count += other.cmd_push_count;
            timeout_retransmitted_count += other.timeout_retransmitted_count;
            fast_retransmitted_count += other.fast_retransmitted_count;
            total_bytes_sent += other.total_bytes_sent;

            return *this;
        }
    };
}