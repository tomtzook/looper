#pragma once

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

#ifdef LOOPER_UNIX_SOCKETS
using unix_socket = handle;
using unix_socket_server = handle;
#endif

struct inet_address;
struct inet_address_view;


struct inet_address_view {
    std::string_view ip;
    uint16_t port{};

    inet_address_view() = default;
    inet_address_view(std::string_view ip, uint16_t port);
    inet_address_view(inet_address_view&) = default;
    inet_address_view(inet_address_view&&) = default;

    // ReSharper disable once CppNonExplicitConvertingConstructor
    inet_address_view(const inet_address&); // NOLINT(*-explicit-constructor)
    inet_address_view& operator=(const inet_address&);
};

struct inet_address {
    std::string ip;
    uint16_t port{};

    inet_address() = default;
    inet_address(std::string_view ip, uint16_t port);
    inet_address(inet_address&) = default;
    inet_address(inet_address&&) = default;

    // ReSharper disable once CppNonExplicitConvertingConstructor
    inet_address(const inet_address_view&); // NOLINT(*-explicit-constructor)
    inet_address& operator=(const inet_address_view&);
};

using loop_callback = std::function<void(loop)>;
using future_callback = std::function<void(future)>;
using event_callback = std::function<void(event)>;
using timer_callback = std::function<void(timer)>;
using read_callback = std::function<void(handle, std::span<const uint8_t>, error)>;
using write_callback = std::function<void(handle, error)>;
using connect_callback = std::function<void(handle, error)>;
using listen_callback = std::function<void(handle)>;
using udp_callback = std::function<void(udp, error)>;
using udp_read_callback = std::function<void(udp, inet_address_view, std::span<const uint8_t>, error)>;

#ifdef LOOPER_UNIX_SOCKETS
using unix_socket_callback = std::function<void(unix_socket, error)>;
using unix_socket_server_callback = std::function<void(unix_socket_server)>;
#endif

enum : error {
    error_success = 0,
    error_eof,
    error_fd_closed,
    error_again,
    error_in_progress,
    error_interrupted,
    error_operation_not_supported,
    error_allocation,
    error_invalid_filemode
};

}
