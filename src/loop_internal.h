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
#include <looper_trace.h>

#include "util/handles.h"
#include "util/util.h"
#include "poll.h"
#include "os/factory.h"
#include "loop.h"

namespace looper::impl {

#define loop_log_module "loop"

using resource_callback = std::function<void(loop_context*, void*, event_types)>;

enum class events_update_type {
    override,
    append,
    remove
};

struct resource_data {
    explicit resource_data(resource handle)
        : our_handle(handle)
        , user_ptr(nullptr)
        , resource_obj(nullptr)
        , descriptor(-1)
        , events(0)
        , callback(nullptr)
    {}

    resource our_handle;
    void* user_ptr;
    std::shared_ptr<os::resource> resource_obj;
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

    std::mutex m_mutex;
    std::unique_ptr<poller> m_poller;
    std::chrono::milliseconds m_timeout;
    std::shared_ptr<os::event> m_run_loop_event;

    bool stop;
    bool executing;
    std::condition_variable m_run_finished;

    handles::handle_table<resource_data, 256> m_resource_table;
    std::unordered_map<os::descriptor, resource_data*> m_descriptor_map;
    std::list<future_data*> m_futures;
    std::list<timer_data*> m_timers;

    std::deque<update> m_updates;

    std::vector<std::pair<timer_data*, timer_data::loop_callback>> m_timer_call_holder;
    std::vector<std::pair<future_data*, future_data::loop_callback>> m_future_call_holder;
    uint8_t m_read_buffer[2048];
};

static constexpr size_t max_events_for_process = 20;
static constexpr size_t initial_reserve_size = 20;
static constexpr auto initial_poll_timeout = std::chrono::milliseconds(1000);
static constexpr auto min_poll_timeout = std::chrono::milliseconds(100);

std::chrono::milliseconds time_now();
void signal_run(loop_context* context);

resource add_resource(loop_context* context, std::shared_ptr<os::resource> resource,
                      event_types events,
                      resource_callback&& callback,
                      void* user_ptr = nullptr);
void remove_resource(loop_context* context, resource resource);
void request_resource_events(loop_context* context, resource resource, event_types events,
                             events_update_type type = events_update_type::override);

}
