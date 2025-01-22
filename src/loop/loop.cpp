
#include "looper_trace.h"

#include "os/factory.h"
#include "loop.h"
#include "loop_internal.h"

namespace looper::impl {

#define log_module loop_log_module

static void run_signal_resource_handler(loop_context* context, void* data, event_types events) {
    os::event::clear(context->run_loop_event.get());
}

static void event_resource_handler(loop_context* context, void* data, event_types events) {
    std::unique_lock lock(context->mutex);

    auto* event = reinterpret_cast<event_data*>(data);
    if (event->resource == empty_handle) {
        return;
    }

    invoke_func(lock, "event_callback", event->from_loop_callback, event);
}

static void reset_smallest_timeout(loop_context* context) {
    std::chrono::milliseconds timeout = initial_poll_timeout;
    for (auto* timer : context->timers) {
        if (timer->timeout < timeout) {
            timeout = timer->timeout;
        }
    }

    context->timeout = timeout;
}

static void process_timers(loop_context* context, std::unique_lock<std::mutex>& lock) {
    std::vector<std::pair<timer_data*, timer_data::loop_callback>> to_call;

    const auto now = time_now();
    for (auto* timer : context->timers) {
        if (!timer->running || timer->hit || timer->next_timestamp > now) {
            continue;
        }

        looper_trace_debug(log_module, "timer hit: ptr=0x%x", timer);

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
    for (auto* future : context->futures) {
        if (future->finished || future->execute_time > now) {
            continue;
        }

        looper_trace_debug(log_module, "future finished: ptr=0x%x", future);

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
    add_resource(context.get(),
                 os::event::get_descriptor(context->run_loop_event.get()),
                 event_in, run_signal_resource_handler);

    return context.release();
}

void destroy_loop(loop_context* context) {
    std::unique_lock lock(context->mutex);

    looper_trace_info(log_module, "stopping looper");

    context->stop = true;
    signal_run(context);

    if (context->executing) {
        context->run_finished.wait(lock, [context]()->bool {
            return !context->executing;
        });
    }

    lock.unlock();
    // todo: there might still be a race here with someone trying to take the lock
    delete context;
}

// events
void add_event(loop_context* context, event_data* event) {
    std::unique_lock lock(context->mutex);

    auto resource = add_resource(context,
                                 os::event::get_descriptor(event->event_obj.get()),
                                 event_in, event_resource_handler, event);
    event->resource = resource;

    looper_trace_info(log_module, "added event: ptr=0x%x, resource_handle=%lu", event, resource);
}

void remove_event(loop_context* context, event_data* event) {
    std::unique_lock lock(context->mutex);

    looper_trace_info(log_module, "removing event: ptr=0x%x, resource_handle=%lu", event, event->resource);

    if (event->resource != empty_handle) {
        remove_resource(context, event->resource);
        event->resource = empty_handle;
    }
}

// timer
void add_timer(loop_context* context, timer_data* timer) {
    std::unique_lock lock(context->mutex);

    if (timer->timeout < min_poll_timeout) {
        throw std::runtime_error("timer timeout too small");
    }

    timer->running = true;
    timer->hit = false;
    timer->next_timestamp = time_now() + timer->timeout;

    context->timers.push_back(timer);

    looper_trace_info(log_module, "starting timer: ptr=0x%x, next_time=%lu", timer, timer->next_timestamp.count());

    if (context->timeout > timer->timeout) {
        context->timeout = timer->timeout;
    }

    signal_run(context);
}

void remove_timer(loop_context* context, timer_data* timer) {
    std::unique_lock lock(context->mutex);

    looper_trace_info(log_module, "removing timer: ptr=0x%x", timer);

    context->timers.remove(timer);
    reset_smallest_timeout(context);
}

void reset_timer(loop_context* context, timer_data* timer) {
    std::unique_lock lock(context->mutex);

    timer->hit = false;
    timer->next_timestamp = time_now() + timer->timeout;

    looper_trace_info(log_module, "resetting timer: ptr=0x%x, next_time=%lu", timer, timer->next_timestamp.count());
}

// execute
void add_future(loop_context* context, future_data* future) {
    std::unique_lock lock(context->mutex);

    future->finished = true;
    context->futures.push_back(future);

    looper_trace_info(log_module, "running new future: ptr=0x%x, run_at=%lu", future, future->execute_time.count());
}

void remove_future(loop_context* context, future_data* future) {
    std::unique_lock lock(context->mutex);

    looper_trace_info(log_module, "removing future: ptr=0x%x", future);

    context->futures.remove(future);
}

void exec_future(loop_context* context, future_data* future) {
    std::unique_lock lock(context->mutex);

    future->finished = false;
    future->execute_time = time_now() + future->delay;

    looper_trace_info(log_module, "queueing future: ptr=0x%x, run_at=%lu", future, future->execute_time.count());

    if (future->delay.count() < 1) {
        signal_run(context);
    }
}

// run
bool run_once(loop_context* context) {
    std::unique_lock lock(context->mutex);

    if (context->stop) {
        looper_trace_debug(log_module, "looper marked stop, not running");
        return true;
    }

    context->executing = true;
    looper_trace_debug(log_module, "start looper run");

    process_updates(context);

    size_t event_count;
    lock.unlock();
    {
        auto status = os::poll::poll(context->poller.get(),
                                     max_events_for_process,
                                     context->timeout,
                                     context->event_data,
                                     event_count);
        if (status == error_interrupted) {
            // timeout
            event_count = 0;
        } else if (status != error_success) {
            looper_trace_error(log_module, "failed to poll: code=%lu", status);
            std::abort();
        }
    }
    lock.lock();

    if (event_count != 0) {
        process_events(context, lock, event_count);
    }

    process_timers(context, lock);
    process_futures(context, lock);

    looper_trace_debug(log_module, "finish looper run");
    context->executing = false;
    context->run_finished.notify_all();

    return context->stop;
}

}
