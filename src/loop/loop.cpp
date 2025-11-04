
#include "looper_trace.h"

#include "os/factory.h"
#include "loop.h"

namespace looper::impl {

#define log_module loop_log_module

static void process_timers(const loop_context* context, std::unique_lock<std::mutex>& lock) {
    std::vector<loop_timer_callback> to_call;

    const auto now = time_now();
    for (auto* timer : context->timers) {
        if (timer->hit || timer->next_timestamp > now) {
            continue;
        }

        looper_trace_debug(log_module, "timer hit: ptr=0x%x", timer);

        timer->hit = true;
        to_call.push_back(timer->callback);
    }

    lock.unlock();
    for (const auto& callback : to_call) {
        invoke_func_nolock("timer_callback", callback);
    }
    lock.lock();
}

static void process_futures(const loop_context* context, std::unique_lock<std::mutex>& lock) {
    std::vector<loop_future_callback> to_call;

    const auto now = time_now();
    for (auto* future : context->futures) {
        if (future->finished || future->execute_time > now) {
            continue;
        }

        looper_trace_debug(log_module, "future finished: ptr=0x%x", future);

        future->finished = true;
        to_call.push_back(future->callback);
    }

    lock.unlock();
    for (const auto& callback : to_call) {
        invoke_func_nolock("future_callback", callback);
    }
    lock.lock();
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
        const auto status = os::poll::poll(
            context->poller.get(),
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
