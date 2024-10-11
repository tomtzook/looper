#pragma once

#include <memory>
#include <deque>
#include <mutex>
#include <unordered_map>
#include <condition_variable>
#include <span>

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
        , write_pending(false)
        , socket_obj()
        , connect_callback()
        , read_callback()
        , write_callback()
    {}

    tcp handle;
    looper::impl::resource resource;
    state state;
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

// tcp
tcp create_tcp(loop_context* context);
void destroy_tcp(loop_context* context, tcp tcp);
void bind_tcp(loop_context* context, tcp tcp, uint16_t port);
void connect_tcp(loop_context* context, tcp tcp, std::string_view server_address, uint16_t server_port, tcp_callback&& callback);
void start_tcp_read(loop_context* context, tcp tcp, tcp_read_callback&& callback);
void write_tcp(loop_context* context, tcp tcp, std::span<const uint8_t> buffer, tcp_callback&& callback);

// run
bool run_once(loop_context* context);
void run_forever(loop_context* context);

}
