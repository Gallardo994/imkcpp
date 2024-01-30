#pragma once

#include "types/cmd.hpp"

namespace imkcpp::commands {
    constexpr Cmd PUSH{81};
    constexpr Cmd ACK{82};
    constexpr Cmd WASK{83};
    constexpr Cmd WINS{84};

    static constexpr bool is_valid(const Cmd cmd) {
        return cmd == PUSH || cmd == ACK || cmd == WASK || cmd == WINS;
    }
}