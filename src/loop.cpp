
#include <looper_trace.h>

#include "os/except.h"
#include "os/factory.h"
#include "loop.h"
#include "loop_internal.h"

namespace looper::impl {

#define log_module loop_log_module

static void run_signal_resource_handler(loop_context* context, void* data, event_types events) {
    context->m_run_loop_event->clear();
}

static void event_resource_handler(loop_context* context, void* data, event_types events) {
    std::unique_lock lock(context->m_mutex);

    auto* event = reinterpret_cast<event_data*>(data);
    if (event->resource == empty_handle) {
        return;
    }

    invoke_func(lock, "event_callback", event->from_loop_callback, event);
}

static void reset_smallest_timeout(loop_context* context) {
    std::chrono::milliseconds timeout = initial_poll_timeout;
    for (auto* timer : context->m_timers) {
        if (timer->timeout < timeout) {
            timeout = timer->timeout;
        }
    }

    context->m_timeout = timeout;
}

static void process_update(loop_context* context, update& update) {
    if (!context->m_resource_table.has(update.handle)) {
        return;
    }

    auto& data = context->m_resource_table[update.handle];

    switch (update.type) {
        case update::type_add:
            data.events = update.events;
            context->m_poller->add(data.descriptor, data.events);
            break;
        case update::type_new_events:
            data.events = update.events;
            context->m_poller->set(data.descriptor, data.events);
            break;
        case update::type_new_events_add:
            data.events |= update.events;
            context->m_poller->set(data.descriptor, data.events);
            break;
        case update::type_new_events_remove:
            data.events &= ~update.events;
            context->m_poller->set(data.descriptor, data.events);
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
            // make sure to remove this fd, guess it somehow was left over
            context->m_poller->remove(descriptor);
            continue;
        }

        auto* data = it->second;

        auto adjusted_flags = (data->events & revents);
        if (adjusted_flags == 0) {
            continue;
        }

        invoke_func(lock, "resource_callback",
                    data->callback, context, data->user_ptr, adjusted_flags);
    }
}

static void process_timers(loop_context* context, std::unique_lock<std::mutex>& lock) {
    std::vector<std::pair<timer_data*, timer_data::loop_callback>> to_call;

    const auto now = time_now();
    for (auto* timer : context->m_timers) {
        if (!timer->running || timer->hit || timer->next_timestamp > now) {
            continue;
        }

        timer->hit = true;
        to_call.push_back({timer, timer->from_loop_callback});
    }

    lock.unlock();
    for (const auto& [timer, callback] : to_call) {
        invoke_func_nolock("timer_callback", callback, timer);
    }
    lock.lock();
}

static void process_futures(loop_context* context, std::unique_lock<std::mutex>& lock) {
    std::vector<std::pair<future_data*, future_data::loop_callback>> to_call;

    const auto now = time_now();
    for (auto* future : context->m_futures) {
        if (future->finished || future->execute_time > now) {
            continue;
        }

        future->finished = true;
        to_call.push_back({future, future->from_loop_callback});
    }

    lock.unlock();
    for (const auto& [future, callback] : to_call) {
        invoke_func_nolock("future_callback", callback, future);
    }
    lock.lock();
}

loop_context* create_loop() {
    looper_trace_info(log_module, "creating looper");

    auto context = std::make_unique<loop_context>();
    add_resource(context.get(), context->m_run_loop_event, event_in, run_signal_resource_handler);

    return context.release();
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
void add_event(loop_context* context, event_data* event) {
    std::unique_lock lock(context->m_mutex);

    auto resource = add_resource(context, event->event_obj, event_in, event_resource_handler, event);
    event->resource = resource;

    looper_trace_info(log_module, "added event: ptr=0x%x resource_handle=%lu", event, resource);
}

void remove_event(loop_context* context, event_data* event) {
    std::unique_lock lock(context->m_mutex);

    looper_trace_info(log_module, "removing event: ptr=0x%x resource_handle=%lu", event, event->resource);

    if (event->resource != empty_handle) {
        remove_resource(context, event->resource);
        event->resource = empty_handle;
    }
}

// timer
void add_timer(loop_context* context, timer_data* timer) {
    std::unique_lock lock(context->m_mutex);

    if (timer->timeout < min_poll_timeout) {
        throw std::runtime_error("timer timeout too small");
    }

    timer->running = true;
    timer->hit = false;
    timer->next_timestamp = time_now() + timer->timeout;

    context->m_timers.push_back(timer);

    looper_trace_info(log_module, "starting timer: ptr=0x%x next_time=%lu", timer, timer->next_timestamp.count());

    if (context->m_timeout > timer->timeout) {
        context->m_timeout = timer->timeout;
    }

    signal_run(context);
}

void remove_timer(loop_context* context, timer_data* timer) {
    std::unique_lock lock(context->m_mutex);

    looper_trace_info(log_module, "removing timer: ptr=0x%x", timer);

    context->m_timers.remove(timer);
    reset_smallest_timeout(context);
}

void reset_timer(loop_context* context, timer_data* timer) {
    std::unique_lock lock(context->m_mutex);

    timer->hit = false;
    timer->next_timestamp = time_now() + timer->timeout;

    looper_trace_info(log_module, "resetting timer: ptr=0x%x next_time=%lu", timer, timer->next_timestamp.count());
}

// execute
void add_future(loop_context* context, future_data* future) {
    std::unique_lock lock(context->m_mutex);

    future->finished = true;
    context->m_futures.push_back(future);

    looper_trace_info(log_module, "running new future: ptr=0x%x, run_at=%lu", future, future->execute_time.count());
}

void remove_future(loop_context* context, future_data* future) {
    std::unique_lock lock(context->m_mutex);

    looper_trace_info(log_module, "removing future: ptr=0x%x", future);

    context->m_futures.remove(future);
}

void exec_future(loop_context* context, future_data* future) {
    std::unique_lock lock(context->m_mutex);

    // todo: there is no guarantee to execute exactly after the delay

    future->finished = false;
    future->execute_time = time_now() + future->delay;

    looper_trace_info(log_module, "queueing future: ptr=0x%x run_at=%lu", future, future->execute_time.count());

    if (future->delay.count() < 1) {
        signal_run(context);
    }
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

}
