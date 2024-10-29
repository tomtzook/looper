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

#include <looper_types.h>

#include "util/handles.h"
#include "poll.h"

namespace looper::impl {

struct loop_context;

using resource = handle;
using resource_id = uint32_t;

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
        , event_obj(nullptr)
        , user_callback(nullptr)
        , resource(empty_handle)
        , from_loop_callback(nullptr)
    {}

    event handle;

    std::shared_ptr<os::event> event_obj;
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
    };

    using loop_callback = std::function<void(tcp_data*, cause, cause_data&, looper::error)>;

    enum class state {
        init,
        errored,
        open,
        connecting,
        connected
    };

    explicit tcp_data(tcp handle)
        : handle(handle)
        , socket_obj(nullptr)
        , connect_callback(nullptr)
        , read_callback(nullptr)
        , write_callback(nullptr)
        , resource(empty_handle)
        , state(state::init)
        , reading(false)
        , write_pending(false)
        , callback(nullptr)
    {}

    tcp handle;

    std::shared_ptr<os::tcp_socket> socket_obj;
    tcp_callback connect_callback;
    tcp_read_callback read_callback;
    tcp_callback write_callback;

    // managed in loop
    looper::impl::resource resource;
    state state;
    bool reading;
    bool write_pending;
    loop_callback callback;
};

}
