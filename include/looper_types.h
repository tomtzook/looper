#pragma once

#include <cstdint>
#include <functional>

namespace looper {

using handle = uint32_t;
static constexpr handle empty_handle = static_cast<handle>(-1);

using loop = handle;
using resource = handle;

// using timer = handle;
// using event = handle;

using event_types = uint32_t;

enum event_type : event_types {
    event_in = (0x1 << 0),
    event_out = (0x1 << 1),
    event_error = (0x1 << 2),
    event_hung = (0x1 << 3)
};

using resource_callback = std::function<void(loop loop, resource resource, event_types)>;
using execute_callback = std::function<void(loop loop)>;

}
