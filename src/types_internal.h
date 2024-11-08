#pragma once

#include <looper_types.h>

namespace looper {

using event_types = uint32_t;

enum event_type : event_types {
    event_in = (0x1 << 0),
    event_out = (0x1 << 1),
    event_error = (0x1 << 2),
    event_hung = (0x1 << 3)
};

}
