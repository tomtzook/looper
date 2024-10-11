#pragma once

#include <memory>
#include <deque>
#include <mutex>
#include <unordered_map>

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
    looper::resource resource;
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

struct execute_request {
    execute_callback callback;
};

struct loop_context {
    explicit loop_context(loop handle);

    loop m_handle;
    std::mutex m_mutex;
    std::unique_ptr<poller> m_poller;
    std::chrono::milliseconds m_timeout;
    std::shared_ptr<os::event> m_run_loop_event;

    handles::handle_table<event_data, 64> m_event_table;
    handles::handle_table<timer_data, 64> m_timer_table;
    handles::handle_table<resource_data, 256> m_resource_table;
    std::unordered_map<os::descriptor, resource_data*> m_descriptor_map;

    std::deque<update> m_updates;
    std::deque<execute_request> m_execute_requests;
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
void execute_later(loop_context* context, execute_callback&& callback, bool wait);

// run
void run_once(loop_context* context);

}
