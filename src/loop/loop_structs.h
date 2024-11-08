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
    enum class cause {
        connect,
        write_finished,
        read,
        error
    };
    union cause_data {
        struct {
            std::span<const uint8_t> data;
        } read;
        struct {
            tcp_callback callback = nullptr;
        } write;
    };
    struct write_request {
        std::unique_ptr<uint8_t[]> buffer;
        size_t pos;
        size_t size;
        tcp_callback write_callback;
    };

    using loop_read_callback = std::function<void(tcp_data*, std::span<const uint8_t>, looper::error)>;
    using loop_write_callback = std::function<void(tcp_data*, write_request&, looper::error)>;
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
        , connect_callback(nullptr)
        , read_callback(nullptr)
        , write_requests()
        , resource(empty_handle)
        , state(state::init)
        , reading(false)
        , write_pending(false)
        , l_read_callback(nullptr)
        , l_write_callback(nullptr)
        , l_connect_callback(nullptr)
        , l_error_callback(nullptr)
    {}

    tcp handle;

    os::tcp_ptr socket_obj;
    tcp_callback connect_callback;
    tcp_read_callback read_callback;
    std::deque<write_request> write_requests;

    // managed in loop
    looper::impl::resource resource;
    state state;
    bool reading;
    bool write_pending;
    loop_read_callback l_read_callback; // todo: too many callbacks
    loop_write_callback l_write_callback;
    loop_callback l_connect_callback;
    loop_callback l_error_callback;
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

}
