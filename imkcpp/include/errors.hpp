#pragma once

namespace imkcpp {
    enum class error {
        none = 0,
        peek_error = 1,
        buffer_too_small = 2,
        queue_empty = 3,
        waiting_for_fragment = 4,
        buffer_empty = 5,
        too_many_fragments = 6,
        less_than_header_size = 7,
        more_than_mtu = 8,
        conv_mismatch = 9,
        header_and_payload_length_mismatch = 10,
        unknown_command = 11,
    };
}