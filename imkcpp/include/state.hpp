#pragma once

namespace imkcpp {
    enum class State : i32 {
        Alive = 0,
        DeadLink = 1,
        // TODO: Add Finalize state which will be used to close the connection gracefully
        // TODO: by allowing only existing queued data and ACKs to be sent and received.
    };
}