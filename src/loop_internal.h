#pragma once

#include <memory>
#include <deque>
#include <mutex>
#include <unordered_map>
#include <condition_variable>
#include <span>
#include <chrono>

#include <looper_types.h>
#include <looper_trace.h>

#include "util/handles.h"
#include "poll.h"
#include "os/factory.h"
#include "loop.h"

namespace looper::impl {

#define loop_log_module "loop"

enum class events_update_type {
    override,
    append,
    remove
};

using resource = handle;
using resource_callback = std::function<void(loop_context*, resource, handle, event_types)>;

struct resource_data {
    explicit resource_data(resource handle)
        : our_handle(handle)
        , external_handle(empty_handle)
        , resource_obj(nullptr)
        , descriptor(-1)
        , events(0)
        , callback()
    {}

    resource our_handle;
    handle external_handle;
    std::shared_ptr<os::resource> resource_obj;
    os::descriptor descriptor;
    event_types events;
    resource_callback callback;
};

struct event_data {
    explicit event_data(event handle)
        : handle(handle)
        , resource(empty_handle)
        , event_obj(nullptr)
        , callback()
    {}

    looper::event handle;
    looper::impl::resource resource;
    std::shared_ptr<os::event> event_obj;
    event_callback callback;
};

struct timer_data {
    explicit timer_data(timer handle)
        : handle(handle)
        , running(false)
        , hit(false)
        , timeout(0)
        , next_timestamp(0)
        , callback()
    {}

    looper::timer handle;
    bool running;
    bool hit;
    std::chrono::milliseconds timeout;
    std::chrono::milliseconds next_timestamp;
    timer_callback callback;
};

struct future_data {
    explicit future_data(future handle)
        : handle(handle)
        , finished(true)
        , remove(false)
        , execute_time(0)
        , callback()
    {}

    future handle;
    bool finished;
    bool remove;
    std::chrono::milliseconds execute_time;
    future_callback callback;
};

struct tcp_data {
    enum class state {
        init,
        errored,
        open,
        connecting,
        connected
    };

    explicit tcp_data(tcp handle)
        : handle(handle)
        , resource(empty_handle)
        , state(state::init)
        , reading(false)
        , write_pending(false)
        , socket_obj()
        , connect_callback()
        , read_callback()
        , write_callback()
    {}

    tcp handle;
    looper::impl::resource resource;
    state state;
    bool reading;
    bool write_pending;
    std::shared_ptr<os::tcp_socket> socket_obj;
    tcp_callback connect_callback;
    tcp_read_callback read_callback;
    tcp_callback write_callback;
};

struct update {
    enum update_type {
        type_add,
        type_new_events,
        type_new_events_add,
        type_new_events_remove,
    };

    resource handle;
    update_type type;
    event_types events;
};

struct loop_context {
    explicit loop_context(loop handle);

    loop m_handle;
    std::mutex m_mutex;
    std::unique_ptr<poller> m_poller;
    std::chrono::milliseconds m_timeout;
    std::shared_ptr<os::event> m_run_loop_event;
    std::condition_variable m_future_executed;

    bool stop;
    bool executing;
    std::condition_variable m_run_finished;

    handles::handle_table<future_data, 64> m_future_table;
    handles::handle_table<event_data, 64> m_event_table;
    handles::handle_table<timer_data, 64> m_timer_table;
    handles::handle_table<tcp_data, 64> m_tcp_table;
    handles::handle_table<resource_data, 256> m_resource_table;
    std::unordered_map<os::descriptor, resource_data*> m_descriptor_map;

    std::deque<update> m_updates;

    uint8_t m_read_buffer[2048];
};

std::chrono::milliseconds time_now();
void signal_run(loop_context* context);

resource add_resource(loop_context* context, std::shared_ptr<os::resource> resource,
                      event_types events,
                      resource_callback&& callback,
                      handle external_handle = empty_handle);
void remove_resource(loop_context* context, resource resource);
void request_resource_events(loop_context* context, resource resource, event_types events,
                             events_update_type type = events_update_type::override);

template<typename _mutex, typename... args_>
static void invoke_func(std::unique_lock<_mutex>& lock, const char* name, const std::function<void(args_...)>& ref, args_... args) {
    if (ref != nullptr) {
        lock.unlock();
        try {
            ref(args...);
        } catch (const std::exception& e) {
            looper_trace_error(loop_log_module, "Error while invoking func %s: what=%s", name, e.what());
        } catch (...) {
            looper_trace_error(loop_log_module, "Error while invoking func %s: unknown", name);
        }
        lock.lock();
    }
}

}
