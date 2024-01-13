#pragma once

#include <string>

namespace imkcpp {
    enum class error {
        none = 0,
        peek_error = 1,
        buffer_too_small = 2,
        queue_empty = 3,
        waiting_for_fragment = 4,
        too_many_fragments = 5,
        less_than_header_size = 6,
        more_than_mtu = 7,
        conv_mismatch = 8,
        header_and_payload_length_mismatch = 9,
        unknown_command = 10,
        exceeds_window_size = 11,
    };

    inline std::string err_to_str(error e) {
        switch (e) {
            case error::none:
                return "none";
            case error::peek_error:
                return "peek_error";
            case error::buffer_too_small:
                return "buffer_too_small";
            case error::queue_empty:
                return "queue_empty";
            case error::waiting_for_fragment:
                return "waiting_for_fragment";
            case error::too_many_fragments:
                return "too_many_fragments";
            case error::less_than_header_size:
                return "less_than_header_size";
            case error::more_than_mtu:
                return "more_than_mtu";
            case error::conv_mismatch:
                return "conv_mismatch";
            case error::header_and_payload_length_mismatch:
                return "header_and_payload_length_mismatch";
            case error::unknown_command:
                return "unknown_command";
            case error::exceeds_window_size:
                return "exceeds_window_size";
            default:
                return "unknown";
        }
    }
}