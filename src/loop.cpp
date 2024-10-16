
#include <looper_trace.h>

#include "os/except.h"
#include "os/factory.h"
#include "loop.h"
#include "loop_internal.h"

namespace looper::impl {

#define log_module loop_log_module

static constexpr size_t max_events_for_process = 20;
static constexpr auto initial_poll_timeout = std::chrono::milliseconds(1000);
static constexpr auto min_poll_timeout = std::chrono::milliseconds(100);

static void run_signal_resource_handler(loop_context* context, looper::impl::resource resource, handle handle, event_types events) {
    context->m_run_loop_event->clear();
}

static void event_resource_handler(loop_context* context, looper::impl::resource resource, handle handle, event_types events) {
    std::unique_lock lock(context->m_mutex);
    if (!context->m_event_table.has(handle)) {
        return;
    }

    auto event_data = context->m_event_table[handle];
    if (event_data->resource == empty_handle) {
        return;
    }

    invoke_func(lock, "event_callback", event_data->callback, context->m_handle, event_data->handle);
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

        invoke_func(lock, "resource_callback",
                    data->callback, context, data->our_handle, data->external_handle, adjusted_flags);
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

        invoke_func(lock, "timer_callback", data.callback, context->m_handle, data.handle);
    }

    for (auto handle : to_remove) {
        context->m_timer_table.release(handle);
    }
}

static void process_futures(loop_context* context, std::unique_lock<std::mutex>& lock) {
    std::vector<future> to_remove;
    const auto now = time_now();

    for (auto [handle, data] : context->m_future_table) {
        if (data.remove) {
            to_remove.push_back(handle);
            continue;
        }

        if (data.finished || data.execute_time > now) {
            continue;
        }

        data.finished = true;

        invoke_func(lock, "future_callback", data.callback, context->m_handle, data.handle);
    }

    context->m_future_executed.notify_all();

    for (auto handle : to_remove) {
        context->m_future_table.release(handle);
    }
}

loop_context::loop_context(loop handle)
    : m_handle(handle)
    , m_mutex()
    , m_poller(os::create_poller())
    , m_timeout(initial_poll_timeout)
    , m_run_loop_event(os::create_event())
    , m_future_executed()
    , stop(false)
    , executing(false)
    , m_run_finished()
    , m_future_table(handles::handle{handle}.index(), handles::type_future)
    , m_event_table(handles::handle{handle}.index(), handles::type_event)
    , m_timer_table(handles::handle{handle}.index(), handles::type_timer)
    , m_tcp_table(handles::handle{handle}.index(), handles::type_tcp)
    , m_resource_table(handles::handle{handle}.index(), handles::type_resource)
    , m_descriptor_map()
    , m_updates()
    , m_read_buffer()
{}

loop_context* create_loop(loop handle) {
    looper_trace_info(log_module, "creating looper");

    auto context = new loop_context(handle);
    add_resource(context, context->m_run_loop_event, event_in, run_signal_resource_handler);
    // todo: handle release on failure

    return context;
}

void destroy_loop(loop_context* context) {
    std::unique_lock lock(context->m_mutex);

    looper_trace_info(log_module, "stopping looper");

    context->stop = true;
    signal_run(context);

    if (context->executing) {
        context->m_run_finished.wait(lock, [context]()->bool {
            return !context->executing;
        });
    }

    lock.unlock();
    // todo: there might still be a race here with someone trying to take the lock
    delete context;
}

// events
event create_event(loop_context* context, event_callback&& callback) {
    std::unique_lock lock(context->m_mutex);

    auto event = os::create_event();

    auto [handle, data] = context->m_event_table.allocate_new();
    data->event_obj = event;
    data->callback = std::move(callback);

    looper_trace_info(log_module, "creating new event: handle=%lu, fd=%d", handle, event->get_descriptor());

    auto resource = add_resource(context, event, event_in, event_resource_handler, handle);
    data->resource = resource;

    context->m_event_table.assign(handle, std::move(data));

    return handle;
}

void destroy_event(loop_context* context, event event) {
    std::unique_lock lock(context->m_mutex);

    auto data = context->m_event_table.release(event);
    remove_resource(context, data->resource);

    looper_trace_info(log_module, "destroying event: handle=%lu", event);
}

void set_event(loop_context* context, event event) {
    std::unique_lock lock(context->m_mutex);

    auto data = context->m_event_table[event];
    if (data->event_obj) {
        looper_trace_info(log_module, "setting event: handle=%lu", event);
        data->event_obj->set();
    }
}

void clear_event(loop_context* context, event event) {
    std::unique_lock lock(context->m_mutex);

    auto data = context->m_event_table[event];
    if (data->event_obj) {
        looper_trace_info(log_module, "clearing event: handle=%lu", event);
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

    looper_trace_info(log_module, "creating new timer: handle=%lu, timeout=%ul", handle, timeout.count());

    context->m_timer_table.assign(handle, std::move(data));

    if (context->m_timeout > timeout) {
        context->m_timeout = timeout;
    }

    signal_run(context);

    return handle;
}

void destroy_timer(loop_context* context, timer timer) {
    std::unique_lock lock(context->m_mutex);

    // todo: find new smallest timeout

    // let the loop delete this to prevent sync problems with callbacks
    auto data = context->m_timer_table[timer];
    data->timeout = std::chrono::milliseconds(0);
    data->running = false;

    looper_trace_info(log_module, "destroying timer: handle=%lu", timer);
}

void start_timer(loop_context* context, timer timer) {
    std::unique_lock lock(context->m_mutex);

    auto data = context->m_timer_table[timer];
    data->next_timestamp = time_now() + data->timeout;
    data->hit = false;
    data->running = true;

    looper_trace_info(log_module, "starting timer: handle=%lu", timer);
}

void stop_timer(loop_context* context, timer timer) {
    std::unique_lock lock(context->m_mutex);

    auto data = context->m_timer_table[timer];
    data->running = false;

    looper_trace_info(log_module, "stopping timer: handle=%lu", timer);
}

void reset_timer(loop_context* context, timer timer) {
    std::unique_lock lock(context->m_mutex);

    auto data = context->m_timer_table[timer];
    data->next_timestamp = time_now() + data->timeout;
    data->hit = false;

    looper_trace_info(log_module, "resetting timer: handle=%lu", timer);
}

// execute
future create_future(loop_context* context, future_callback&& callback) {
    std::unique_lock lock(context->m_mutex);

    auto [handle, data] = context->m_future_table.allocate_new();
    data->finished = true;
    data->remove = false;
    data->execute_time = std::chrono::milliseconds(0);
    data->callback = std::move(callback);

    looper_trace_info(log_module, "creating future: handle=%lu", handle);

    context->m_future_table.assign(handle, std::move(data));

    return handle;
}

void destroy_future(loop_context* context, future future) {
    std::unique_lock lock(context->m_mutex);

    auto data = context->m_future_table[future];
    data->remove = true;

    looper_trace_info(log_module, "destroying future: handle=%lu", future);
}

void execute_later(loop_context* context, future future, std::chrono::milliseconds delay) {
    std::unique_lock lock(context->m_mutex);

    if (delay.count() != 0 && delay < min_poll_timeout) {
        throw std::runtime_error("timer timeout too small");
    }

    // todo: find new smallest timeout when finished
    auto data = context->m_future_table[future];
    data->execute_time = time_now() + delay;
    data->finished = false;

    if (delay.count() < 1) {
        signal_run(context);
    }

    looper_trace_info(log_module, "executing for future: handle=%lu, delay=%ul", future, delay.count());
}

bool wait_for(loop_context* context, future future, std::chrono::milliseconds timeout) {
    std::unique_lock lock(context->m_mutex);

    looper_trace_info(log_module, "waiting on future: handle=%lu, timeout=%ul", future, timeout.count());

    if (!context->m_future_table.has(future)) {
        // in case someone is waiting on a removed future because of the race with the callback which removed it
        looper_trace_debug(log_module, "future does not exist, returning");
        return false;
    }

    auto data = context->m_future_table[future];
    if (data->finished) {
        looper_trace_debug(log_module, "future finished, returning");
        return false;
    }

    return !context->m_future_executed.wait_for(lock, timeout, [context, future]()->bool {
        if (!context->m_future_table.has(future)) {
            return true;
        }

        auto data = context->m_future_table[future];
        if (data->finished || data->remove) {
            return true;
        }

        return false;
    });
}

// run
bool run_once(loop_context* context) {
    std::unique_lock lock(context->m_mutex);

    if (context->stop) {
        looper_trace_debug(log_module, "looper marked stop, not running");
        return true;
    }

    context->executing = true;
    looper_trace_debug(log_module, "start looper run");

    process_updates(context);

    lock.unlock();
    auto result = context->m_poller->poll(max_events_for_process, context->m_timeout);
    lock.lock();

    process_events(context, lock, result);
    process_timers(context, lock);
    process_futures(context, lock);

    looper_trace_debug(log_module, "finish looper run");
    context->executing = false;
    context->m_run_finished.notify_all();

    return context->stop;
}

void run_forever(loop_context* context) {
    while (true) {
        bool finished = run_once(context);
        if (finished) {
            break;
        }
    }
}

}
