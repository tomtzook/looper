#pragma once

#include <memory>
#include <deque>
#include <mutex>
#include <unordered_map>
#include <condition_variable>

#include <looper_types.h>

#include "util/handles.h"
#include "poll.h"
#include "os/factory.h"

namespace looper::impl {

enum class events_update_type {
    override,
    append,
    remove
};

using resource = handle;
using resource_callback = std::function<void(loop, resource, event_types)>;

struct resource_data {
    explicit resource_data(resource handle)
        : handle(handle)
        , resource_obj(nullptr)
        , descriptor(-1)
        , events(0)
        , callback()
    {}

    resource handle;
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

    handles::handle_table<future_data, 64> m_future_table;
    handles::handle_table<event_data, 64> m_event_table;
    handles::handle_table<timer_data, 64> m_timer_table;
    handles::handle_table<resource_data, 256> m_resource_table;
    std::unordered_map<os::descriptor, resource_data*> m_descriptor_map;

    std::deque<update> m_updates;
};

loop_context* create_loop(loop handle);
void destroy_loop(loop_context* context);

// event
event create_event(loop_context* context, event_callback&& callback);
void destroy_event(loop_context* context, event event);
void set_event(loop_context* context, event event);
void clear_event(loop_context* context, event event);

// timer
timer create_timer(loop_context* context, std::chrono::milliseconds timeout, timer_callback&& callback);
void destroy_timer(loop_context* context, timer timer);
void start_timer(loop_context* context, timer timer);
void stop_timer(loop_context* context, timer timer);
void reset_timer(loop_context* context, timer timer);

// execute
future create_future(loop_context* context, future_callback&& callback);
void destroy_future(loop_context* context, future future);
void execute_later(loop_context* context, future future, std::chrono::milliseconds delay);
bool wait_for(loop_context* context, future future, std::chrono::milliseconds timeout);

// run
void run_once(loop_context* context);

}
