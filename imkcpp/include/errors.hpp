#pragma once

namespace imkcpp {
    enum class peek_error {
        none = 0,
        queue_empty = 1,
        waiting_for_fragment = 2,
    };

    enum class send_error {
        none = 0,
        buffer_empty = 1,
        too_many_fragments = 2,
    };

    enum class input_error {
        none = 0,
        less_than_header_size = 1,
        conv_mismatch = 2,
        header_and_payload_length_mismatch = 3,
    };

    enum class recv_error {
        none = 0,
        queue_empty = 1,
        peek_error = 2,
        buffer_too_small = 3,
    };
}