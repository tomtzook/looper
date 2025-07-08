#pragma once

#include <memory>
#include <deque>
#include <mutex>
#include <unordered_map>
#include <condition_variable>
#include <span>
#include <chrono>
#include <vector>
#include <list>

#include "looper_types.h"

#include "util/handles.h"
#include "types_internal.h"

namespace looper::impl {

struct loop_context;

using resource = handle;

struct timer_data {
    using loop_callback = std::function<void(timer_data*)>;

    explicit timer_data(timer handle)
        : handle(handle)
        , running(false)
        , hit(false)
        , timeout(0)
        , next_timestamp(0)
        , user_callback(nullptr)
        , from_loop_callback(nullptr)
    {}

    timer handle;

    bool running;
    bool hit;
    std::chrono::milliseconds timeout;
    timer_callback user_callback;

    // managed in loop
    std::chrono::milliseconds next_timestamp;
    loop_callback from_loop_callback;
};

struct future_data {
    using loop_callback = std::function<void(future_data*)>;

    explicit future_data(future handle)
        : handle(handle)
        , delay(0)
        , exec_finished()
        , user_callback(nullptr)
        , finished(true)
        , execute_time(0)
        , from_loop_callback(nullptr)
    {}

    future handle;

    std::chrono::milliseconds delay;
    std::condition_variable exec_finished;
    future_callback user_callback;

    // managed in loop
    bool finished;
    std::chrono::milliseconds execute_time;
    loop_callback from_loop_callback;
};

struct event_data {
    using loop_callback = std::function<void(event_data*)>;

    explicit event_data(event handle)
        : handle(handle)
        , event_obj(nullptr, nullptr)
        , user_callback(nullptr)
        , resource(empty_handle)
        , from_loop_callback(nullptr)
    {}

    event handle;

    os::event_ptr event_obj;
    event_callback user_callback;

    // managed in loop
    looper::impl::resource resource;
    loop_callback from_loop_callback;
};

struct tcp_data {
    struct write_request {
        std::unique_ptr<uint8_t[]> buffer;
        size_t pos;
        size_t size;
        tcp_callback write_callback;

        looper::error error;
    };

    using loop_read_callback = std::function<void(tcp_data*, std::span<const uint8_t>, looper::error)>;
    using loop_write_callback = std::function<void(tcp_data*, write_request&)>;
    using loop_callback = std::function<void(tcp_data*, looper::error)>;

    enum class state {
        init,
        errored,
        open,
        connecting,
        connected
    };

    explicit tcp_data(tcp handle)
        : handle(handle)
        , socket_obj(nullptr, nullptr)
        , user_connect_callback(nullptr)
        , user_read_callback(nullptr)
        , write_requests()
        , completed_write_requests()
        , resource(empty_handle)
        , state(state::init)
        , reading(false)
        , write_pending(false)
        , from_loop_read_callback(nullptr)
        , from_loop_write_callback(nullptr)
        , from_loop_connect_callback(nullptr)
    {}

    tcp handle;

    os::tcp_ptr socket_obj;
    tcp_callback user_connect_callback;
    tcp_read_callback user_read_callback;
    std::deque<write_request> write_requests;
    std::deque<write_request> completed_write_requests;

    // managed in loop
    looper::impl::resource resource;
    state state;
    bool reading;
    bool write_pending;

    loop_read_callback from_loop_read_callback;
    loop_write_callback from_loop_write_callback;
    loop_callback from_loop_connect_callback;
};

struct tcp_server_data {
    using loop_callback = std::function<void(tcp_server_data*)>;

    explicit tcp_server_data(tcp_server handle)
        : handle(handle)
        , socket_obj(nullptr, nullptr)
        , connect_callback(nullptr)
        , resource(empty_handle)
        , callback(nullptr)
    {}

    tcp_server handle;

    os::tcp_ptr socket_obj;
    tcp_server_callback connect_callback;

    // managed in loop
    looper::impl::resource resource;
    loop_callback callback;
};

struct udp_data {
    struct write_request {
        inet_address destination;
        std::unique_ptr<uint8_t[]> buffer;
        size_t size;
        udp_callback write_callback;

        looper::error error;
    };

    using loop_read_callback = std::function<void(udp_data*, const inet_address& sender, std::span<const uint8_t>, looper::error)>;
    using loop_write_callback = std::function<void(udp_data*, write_request&)>;

    enum class state {
        init,
        errored,
        open
    };

    explicit udp_data(udp handle)
        : handle(handle)
        , socket_obj(nullptr, nullptr)
        , write_requests()
        , completed_write_requests()
        , resource(empty_handle)
        , reading(false)
        , write_pending(false)
        , user_read_callback(nullptr)
        , from_loop_read_callback(nullptr)
        , from_loop_write_callback(nullptr)
        , state(state::init)
    {}

    udp handle;

    os::udp_ptr socket_obj;
    udp_read_callback user_read_callback;
    std::deque<write_request> write_requests;
    std::deque<write_request> completed_write_requests;

    // managed in loop
    looper::impl::resource resource;
    bool reading;
    bool write_pending;
    state state;

    loop_read_callback from_loop_read_callback;
    loop_write_callback from_loop_write_callback;
};

}
