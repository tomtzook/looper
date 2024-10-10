#pragma once

#include <memory>
#include <deque>
#include <mutex>
#include <unordered_map>

#include <looper_types.h>

#include "util/handles.h"
#include "poll.h"
#include "factory.h"

namespace looper {

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

class loop_impl {
public:
    explicit loop_impl(loop handle);
    ~loop_impl() = default;

    resource add_resource(std::shared_ptr<os::resource> resource, event_types events, resource_callback&& callback);
    void remove_resource(resource resource);
    void request_resource_events(resource resource, event_types events, events_update_type type = events_update_type::override);

    void execute_later(execute_callback&& callback, bool wait);

    void run_once();

private:
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

    void signal_run();

    void process_updates();
    void process_update(update& update);
    void process_events(std::unique_lock<std::mutex>& lock, polled_events& events);
    void execute_requests(std::unique_lock<std::mutex>& lock);

    loop m_handle;
    std::mutex m_mutex;
    std::unique_ptr<poller> m_poller;
    std::chrono::milliseconds m_timeout;
    std::shared_ptr<os::event> m_run_loop_event;

    handles::handle_table<resource_data, 256> m_resource_table;
    std::unordered_map<os::descriptor, resource_data*> m_descriptor_map;

    std::deque<update> m_updates;
    std::deque<execute_request> m_execute_requests;
};

}
