
#include <looper_trace.h>

#include "os/factory.h"
#include "loop.h"

namespace looper::impl {

#define log_module "loop"

static constexpr size_t max_events_for_process = 20;
static constexpr auto initial_poll_timeout = std::chrono::milliseconds(1000);
static constexpr auto min_poll_timeout = std::chrono::milliseconds(100);

static std::chrono::milliseconds time_now() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch());
}

static void signal_run(loop_context* context) {
    context->m_run_loop_event->set();
}

static resource add_resource(loop_context* context, std::shared_ptr<os::resource> resource, event_types events, resource_callback&& callback) {
    const auto descriptor = resource->get_descriptor();

    auto it = context->m_descriptor_map.find(descriptor);
    if (it != context->m_descriptor_map.end()) {
        throw std::runtime_error("resource already added");
    }

    auto [handle, data] = context->m_resource_table.allocate_new();
    data->resource_obj = std::move(resource);
    data->descriptor = descriptor;
    data->events = 0;
    data->callback = std::move(callback);

    context->m_descriptor_map.emplace(descriptor, data);
    context->m_updates.push_back({handle, update::type_add, events});

    signal_run(context);

    return handle;
}

static void remove_resource(loop_context* context, resource resource) {
    auto data = context->m_resource_table.release(resource);

    context->m_descriptor_map.erase(data->descriptor);
    context->m_poller->remove(data->descriptor);

    signal_run(context);
}

static void request_resource_events(loop_context* context, resource resource, event_types events, events_update_type type = events_update_type::override) {
    auto data = context->m_resource_table[resource];

    update::update_type update_type;
    switch (type) {
        case events_update_type::override:
            update_type = update::type_new_events;
            break;
        case events_update_type::append:
            update_type = update::type_new_events_add;
            break;
        case events_update_type::remove:
            update_type = update::type_new_events_remove;
            break;
        default:
            throw std::runtime_error("unsupported event type");
    }

    context->m_updates.push_back({data->handle, update_type, events});
    signal_run(context);
}

static void process_update(loop_context* context, update& update) {
    if (!context->m_resource_table.has(update.handle)) {
        return;
    }

    auto data = context->m_resource_table[update.handle];

    switch (update.type) {
        case update::type_add:
            data->events = update.events;
            context->m_poller->add(data->descriptor, data->events);
            break;
        case update::type_new_events:
            data->events = update.events;
            context->m_poller->set(data->descriptor, data->events);
            break;
        case update::type_new_events_add:
            data->events |= update.events;
            context->m_poller->set(data->descriptor, data->events);
            break;
        case update::type_new_events_remove:
            data->events &= ~update.events;
            context->m_poller->set(data->descriptor, data->events);
            break;
    }
}

static void process_updates(loop_context* context) {
    while (!context->m_updates.empty()) {
        auto& update = context->m_updates.front();
        process_update(context, update);

        context->m_updates.pop_front();
    }
}

static void process_events(loop_context* context, std::unique_lock<std::mutex>& lock, polled_events& events) {
    for (auto [descriptor, revents] : events) {
        auto it = context->m_descriptor_map.find(descriptor);
        if (it == context->m_descriptor_map.end()) {
            continue;
        }

        auto data = it->second;

        auto adjusted_flags = (data->events & revents);
        if (adjusted_flags == 0) {
            continue;
        }

        lock.unlock();
        try {
            data->callback(context->m_handle, data->handle, adjusted_flags);
        } catch (const std::exception& e) {
            looper_trace_error(log_module, "Error in io callback: what=%s", e.what());
        } catch (...) {
            looper_trace_error(log_module, "Error in io callback: unknown");
        }
        lock.lock();
    }
}

static void process_timers(loop_context* context, std::unique_lock<std::mutex>& lock) {
    std::vector<timer> to_remove;
    const auto now = time_now();

    for (auto [handle, data] : context->m_timer_table) {
        if (data.timeout.count() == 0) {
            to_remove.push_back(handle);
            continue;
        }

        if (!data.running || data.hit || data.next_timestamp > now) {
            continue;
        }

        data.hit = true;

        lock.unlock();
        try {
            data.callback(context->m_handle, handle);
        } catch (const std::exception& e) {
            looper_trace_error(log_module, "Error in timer callback: what=%s", e.what());
        } catch (...) {
            looper_trace_error(log_module, "Error in timer callback: unknown");
        }
        lock.lock();
    }

    for (auto handle : to_remove) {
        context->m_timer_table.release(handle);
    }
}

static void execute_requests(loop_context* context, std::unique_lock<std::mutex>& lock) {
    while (!context->m_execute_requests.empty()) {
        auto& request = context->m_execute_requests.front();

        lock.unlock();
        try {
            request.callback(context->m_handle);
        } catch (const std::exception& e) {
            looper_trace_error(log_module, "Error in request callback: what=%s", e.what());
        } catch (...) {
            looper_trace_error(log_module, "Error in request callback: unknown");
        }
        lock.lock();

        context->m_execute_requests.pop_front();
    }
}

loop_context::loop_context(loop handle)
    : m_handle(handle)
    , m_mutex()
    , m_poller(os::create_poller())
    , m_timeout(initial_poll_timeout)
    , m_run_loop_event(os::create_event())
    , m_event_table(handles::handle{handle}.index(), handles::type_event)
    , m_timer_table(handles::handle{handle}.index(), handles::type_timer)
    , m_resource_table(handles::handle{handle}.index(), handles::type_resource)
    , m_descriptor_map()
    , m_updates()
    , m_execute_requests()
{}

loop_context* create_loop(loop handle) {
    auto context = new loop_context(handle);
    add_resource(context, context->m_run_loop_event, event_in, [context](loop loop, resource resource, event_types events)->void {
        context->m_run_loop_event->clear();
    });
    // todo: handle release on failure

    return context;
}

void destroy_loop(loop_context* context) {
    // todo: implement

    delete context;
}

// events
event create_event(loop_context* context, event_callback&& callback) {
    std::unique_lock lock(context->m_mutex);

    auto event = os::create_event();

    auto [handle, data] = context->m_event_table.allocate_new();
    data->event_obj = event;
    data->callback = std::move(callback);

    // todo: on failure of add_resource we will have the event in the table
    auto resource_callback = [context, handle](looper::loop loop, looper::resource resource, event_types events)->void {
        std::unique_lock lock(context->m_mutex);
        if (!context->m_event_table.has(handle)) {
            return;
        }

        auto event_data = context->m_event_table[handle];
        if (event_data->resource == empty_handle) {
            return;
        }

        lock.unlock();
        try {
            event_data->callback(loop, event_data->handle);
        } catch (const std::exception& e) {
            looper_trace_error(log_module, "Error in event callback: what=%s", e.what());
        } catch (...) {
            looper_trace_error(log_module, "Error in event callback: unknown");
        }
        lock.lock();
    };

    auto resource = add_resource(context, event, event_in, resource_callback);
    data->resource = resource;

    return handle;
}

void destroy_event(loop_context* context, event event) {
    std::unique_lock lock(context->m_mutex);

    auto data = context->m_event_table.release(event);
    remove_resource(context, data->resource);
}

void set_event(loop_context* context, event event) {
    std::unique_lock lock(context->m_mutex);

    auto data = context->m_event_table[event];
    if (data->event_obj) {
        data->event_obj->set();
    }
}

void clear_event(loop_context* context, event event) {
    std::unique_lock lock(context->m_mutex);

    auto data = context->m_event_table[event];
    if (data->event_obj) {
        data->event_obj->clear();
    }
}

// timer
timer create_timer(loop_context* context, std::chrono::milliseconds timeout, timer_callback&& callback) {
    std::unique_lock lock(context->m_mutex);

    if (timeout < min_poll_timeout) {
        throw std::runtime_error("timer timeout too small");
    }

    auto [handle, data] = context->m_timer_table.allocate_new();
    data->running = false;
    data->timeout = timeout;
    data->next_timestamp = std::chrono::milliseconds(0);
    data->callback = std::move(callback);

    if (context->m_timeout > timeout) {
        context->m_timeout = timeout;
    }

    signal_run(context);

    return handle;
}

void destroy_timer(loop_context* context, timer timer) {
    std::unique_lock lock(context->m_mutex);

    // let the loop delete this to prevent sync problems with callbacks
    auto data = context->m_timer_table[timer];
    data->timeout = std::chrono::milliseconds(0);
    data->running = false;
}

void start_timer(loop_context* context, timer timer) {
    std::unique_lock lock(context->m_mutex);

    auto data = context->m_timer_table[timer];
    data->next_timestamp = time_now() + data->timeout;
    data->hit = false;
    data->running = true;
}

void stop_timer(loop_context* context, timer timer) {
    std::unique_lock lock(context->m_mutex);

    auto data = context->m_timer_table[timer];
    data->running = false;
}

void reset_timer(loop_context* context, timer timer) {
    std::unique_lock lock(context->m_mutex);

    auto data = context->m_timer_table[timer];
    data->next_timestamp = time_now() + data->timeout;
    data->hit = false;
}

// execute
void execute_later(loop_context* context, execute_callback&& callback, bool wait) {
    std::unique_lock lock(context->m_mutex);

    execute_request request{};
    request.callback = std::move(callback);
    context->m_execute_requests.push_back(request);

    signal_run(context);

    if (wait) {
        // todo: wait for run
    }
}

// run
void run_once(loop_context* context) {
    std::unique_lock lock(context->m_mutex);

    process_updates(context);

    lock.unlock();
    auto result = context->m_poller->poll(max_events_for_process, context->m_timeout);
    lock.lock();

    process_events(context, lock, result);
    process_timers(context, lock);
    execute_requests(context, lock);
}

}
