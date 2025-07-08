#pragma once

#include <cstdint>
#include <chrono>
#include <functional>
#include <span>

namespace looper {

using handle = uint32_t;
using error = int32_t;

static constexpr auto error_unknown = static_cast<error>(-1);
static constexpr handle empty_handle = static_cast<handle>(-1);
static constexpr auto no_timeout = std::chrono::milliseconds(0);
static constexpr auto no_delay = std::chrono::milliseconds(0);

using loop = handle;
using future = handle;
using event = handle;
using timer = handle;
using tcp = handle;
using tcp_server = handle;
using udp = handle;

struct inet_address {
    std::string_view ip;
    uint16_t port;
};

using loop_callback = std::function<void(loop)>;
using future_callback = std::function<void(loop, future)>;
using event_callback = std::function<void(loop, event)>;
using timer_callback = std::function<void(loop, timer)>;
using tcp_callback = std::function<void(loop, tcp, error)>;
using tcp_read_callback = std::function<void(loop, tcp, std::span<const uint8_t> data, error)>;
using tcp_server_callback = std::function<void(loop, tcp_server)>;
using udp_callback = std::function<void(loop, udp, error)>;
using udp_read_callback = std::function<void(loop, udp, inet_address, std::span<const uint8_t> data, error)>;

enum : error {
    error_success = 0,
    error_eof,
    error_fd_closed,
    error_again,
    error_in_progress,
    error_interrupted,
    error_operation_not_supported,
    error_allocation
};

}
