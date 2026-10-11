#pragma once
#include <cstdint>
#include <type_traits>

namespace ring {

    struct alignas(32)  Tick {
        uint64_t          timestamp_ns;  // exchange wall-clock, nanoseconds since epoch 8 bytes
        char              symbol[4];     // e.g. "SYM0" — points into engine-owned storage  4 bytes
        int32_t           bid_px;        // best bid price, IS MULTIPLIED BY 100, for example 2$ is represented as 200. 4 bytes
        int32_t           ask_px;        // best ask price IS MULTIPLIED BY 100, for example 2$ is represented as 200. 4 bytes
        uint32_t          bid_qty;       // quantity available at best bid , 4 bytes
        uint32_t          ask_qty;       // quantity available at best ask , 4 bytes
        char              padding[4];    // 4 bytes
    };

}