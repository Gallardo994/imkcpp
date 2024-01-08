#pragma once

namespace imkcpp {
    struct FlushResult {
        u32 ack_sent_count = 0;
        u32 cmd_wask_count = 0;
        u32 cmd_wins_count = 0;

        u32 data_sent_count = 0;
        size_t total_bytes_sent = 0;
    };
}