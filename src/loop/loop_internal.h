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
#include "looper_trace.h"

#include "util/handles.h"
#include "util/util.h"
#include "types_internal.h"
#include "os/factory.h"
#include "loop.h"

namespace looper::impl {

#define loop_log_module "loop"

using resource_callback = std::function<void(loop_context*, void*, event_types)>;

static constexpr size_t max_events_for_process = 20;
static constexpr size_t initial_reserve_size = 20;
static constexpr auto initial_poll_timeout = std::chrono::milliseconds(1000);
static constexpr auto min_poll_timeout = std::chrono::milliseconds(100);
static constexpr size_t read_buffer_size = 2048;
static constexpr size_t resource_table_size = 256;

enum class events_update_type {
    override,
    append,
    remove
};

struct resource_data {
    explicit resource_data(resource handle)
        : our_handle(handle)
        , user_ptr(nullptr)
        , descriptor(-1)
        , events(0)
        , callback(nullptr)
    {}

    resource our_handle;
    void* user_ptr;
    os::descriptor descriptor;
    event_types events;
    resource_callback callback;
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
    loop_context();

    std::mutex mutex;
    os::poller_ptr poller;
    std::chrono::milliseconds timeout;
    os::event_ptr run_loop_event;
    os::poll::event_data event_data[max_events_for_process];

    bool stop;
    bool executing;
    std::condition_variable run_finished;

    handles::handle_table<resource_data, resource_table_size> resource_table;
    std::unordered_map<os::descriptor, resource_data*> descriptor_map;
    std::list<future_data*> futures;
    std::list<timer_data*> timers;
    std::deque<update> updates;

    std::vector<std::pair<timer_data*, timer_data::loop_callback>> timer_call_holder;
    std::vector<std::pair<future_data*, future_data::loop_callback>> future_call_holder;
    uint8_t read_buffer[read_buffer_size];
};

std::chrono::milliseconds time_now();
void signal_run(loop_context* context);

resource add_resource(loop_context* context,
                      os::descriptor descriptor,
                      event_types events,
                      resource_callback&& callback,
                      void* user_ptr = nullptr);
void remove_resource(loop_context* context, resource resource);
void request_resource_events(loop_context* context, resource resource, event_types events,
                             events_update_type type = events_update_type::override);

void process_updates(loop_context* context);
void process_events(loop_context* context, std::unique_lock<std::mutex>& lock, size_t event_count);

}
