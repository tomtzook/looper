#pragma once

#include <cstdint>
#include <functional>
#include <span>

namespace looper {

using handle = uint32_t;
using error = uint32_t;
static constexpr handle empty_handle = static_cast<handle>(-1);

using loop = handle;
using future = handle;
using event = handle;
using timer = handle;
using tcp = handle;
using tcp_server = handle;

using loop_callback = std::function<void(loop)>;
using future_callback = std::function<void(loop, future)>;
using event_callback = std::function<void(loop, event)>;
using timer_callback = std::function<void(loop, timer)>;
using tcp_callback = std::function<void(loop, tcp, error)>;
using tcp_read_callback = std::function<void(loop, tcp, std::span<const uint8_t> data, error)>;

}
