#include "looper_base.h"

namespace looper {

#define log_module looper_log_module

looper_data& get_global_loop_data() {
    static looper_data g_instance{};
    return g_instance;
}

static void run_loop(const loop loop, const std::chrono::milliseconds time = no_timeout) {
    const auto end_time = impl::time_now() + time;

    while (true) {
        std::unique_lock lock(get_global_loop_data().m_mutex);
        auto data_opt = try_get_loop(loop);
        if (!data_opt) {
            break;
        }

        if (time > no_timeout) {
            const auto now = impl::time_now();
            if (now >= end_time) {
                break;
            }
        }

        const auto* data = data_opt.value();
        lock.unlock();

        const auto finished = data->m_loop->run_once();
        if (finished) {
            break;
        }
    }
}

static void thread_main(const loop loop) {
    run_loop(loop);
}

static future create_future_internal(const loop loop, future_callback&& callback) {
    auto& data = get_loop(loop);

    auto [handle, future_impl] = data.m_futures.allocate_new(
        data.m_loop, std::move(callback));
    looper_trace_info(log_module, "creating future: loop=%lu, handle=%lu", loop, handle);
    data.m_futures.assign(handle, std::move(future_impl));

    return handle;
}

static void destroy_future_internal(const future future) {
    auto& data = get_loop_from_handle(future);

    looper_trace_info(log_module, "destroying future: loop=%lu, handle=%lu", data.m_handle, future);

    const auto future_impl = data.m_futures.release(future);
}

static void execute_future_internal(const future future, const std::chrono::milliseconds delay) {
    auto& data = get_loop_from_handle(future);

    looper_trace_info(log_module, "requesting future execution: loop=%lu, handle=%lu, delay=%lu", data.m_handle, future, delay.count());

    auto& future_impl = data.m_futures[future];
    future_impl.execute(delay);
}

static bool wait_for_future_internal(std::unique_lock<std::mutex>& lock, const future future, const std::chrono::milliseconds timeout) {
    auto& data = get_loop_from_handle(future);

    auto& future_impl = data.m_futures[future];
    return future_impl.wait_for(lock, timeout);
}

loop create() {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto [handle, data] = get_global_loop_data().m_loops.allocate_new();
    get_global_loop_data().m_loops.assign(handle, std::move(data));

    looper_trace_info(log_module, "created new loop: handle=%lu", handle);

    return handle;
}

void destroy(const loop loop) {
    std::unique_lock lock(get_global_loop_data().m_mutex);
    auto& data = get_loop(loop);
    data.m_closing = true;

    looper_trace_info(log_module, "destroying loop: handle=%lu", loop);

    const auto thread = std::move(data.m_thread);
    lock.unlock();

    if (thread && thread->joinable()) {
        looper_trace_debug(log_module, "loop running in thread, joining: handle=%lu", loop);
        thread->join();
    }

    data.clear_context();
    lock.lock();

    get_global_loop_data().m_loops.release(loop);

    looper_trace_info(log_module, "loop destroyed: handle=%lu", loop);
}

loop get_parent_loop(const handle handle) {
    std::unique_lock lock(get_global_loop_data().m_mutex);
    const auto& data = get_loop_from_handle(handle);

    return data.m_handle;
}

void run_once(const loop loop) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    const auto& data = get_loop(loop);
    if (data.m_thread) {
        throw std::runtime_error("loop running in thread");
    }

    looper_trace_debug(log_module, "running loop once: handle=%lu", loop);

    lock.unlock();
    data.m_loop->run_once();
}

void run_for(const loop loop, const std::chrono::milliseconds time) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    const auto& data = get_loop(loop);
    if (data.m_thread) {
        throw std::runtime_error("loop running in thread");
    }

    looper_trace_debug(log_module, "running loop for time: handle=%lu, time=%lu", loop, time.count());

    lock.unlock();
    run_loop(loop, time);
}

void run_forever(const loop loop) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    const auto& data = get_loop(loop);
    if (data.m_thread) {
        throw std::runtime_error("loop running in thread");
    }

    looper_trace_info(log_module, "running loop forever: handle=%lu", loop);

    lock.unlock();
    run_loop(loop);
}

void exec_in_thread(loop loop) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop(loop);
    if (data.m_thread) {
        looper_trace_debug(log_module, "loop already running in thread: handle=%lu", loop);
        return;
    }

    looper_trace_info(log_module, "starting loop execution in thread: handle=%lu", loop);

    data.m_thread = std::make_unique<std::thread>(&thread_main, loop);
}

// execute
future create_future(const loop loop, future_callback&& callback) {
    std::unique_lock lock(get_global_loop_data().m_mutex);
    return create_future_internal(loop, std::move(callback));
}

void destroy_future(const future future) {
    std::unique_lock lock(get_global_loop_data().m_mutex);
    destroy_future_internal(future);
}

void execute_once(const future future, const std::chrono::milliseconds delay) {
    std::unique_lock lock(get_global_loop_data().m_mutex);
    execute_future_internal(future, delay);
}

bool wait_for(const future future, const std::chrono::milliseconds timeout) {
    std::unique_lock lock(get_global_loop_data().m_mutex);
    return wait_for_future_internal(lock, future, timeout);
}

void execute_later(const loop loop, loop_callback&& callback) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    const auto future = create_future_internal(loop, [callback](const looper::future future_cb)->void {
        std::unique_lock lock_cb(get_global_loop_data().m_mutex);
        destroy_future_internal(future_cb);

        invoke_func(lock_cb, "future_singleuse_callback", callback, future_cb);
    });
    execute_future_internal(future, no_delay);
}

bool execute_later_and_wait(const loop loop, loop_callback&& callback, const std::chrono::milliseconds timeout) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    const auto future = create_future_internal(loop, [callback](const looper::future future_cb)->void {
        std::unique_lock lock_cb(get_global_loop_data().m_mutex);
        destroy_future_internal(future_cb);

        invoke_func(lock_cb, "future_singleuse_callback", callback, future_cb);
    });
    execute_future_internal(future, no_delay);

    return wait_for_future_internal(lock, future, timeout);
}

// events
event create_event(const loop loop, event_callback&& callback) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop(loop);

    auto [handle, event_data] = data.m_events.allocate_new(data.m_loop, std::move(callback));
    looper_trace_info(log_module, "created new event: loop=%lu, handle=%lu", loop, handle);
    data.m_events.assign(handle, std::move(event_data));

    return handle;
}

void destroy_event(const event event) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop_from_handle(event);

    looper_trace_info(log_module, "destroying event: loop=%lu, handle=%lu", data.m_handle, event);

    const auto event_impl = data.m_events.release(event);
}

void set_event(const event event) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop_from_handle(event);

    looper_trace_debug(log_module, "setting event: loop=%lu, handle=%lu", data.m_handle, event);

    auto& event_impl = data.m_events[event];
    event_impl.set();
}

void clear_event(const event event) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop_from_handle(event);

    looper_trace_debug(log_module, "clearing event: loop=%lu, handle=%lu", data.m_handle, event);

    auto& event_impl = data.m_events[event];
    event_impl.clear();
}

// timers
timer create_timer(const loop loop, std::chrono::milliseconds timeout, timer_callback&& callback) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop(loop);

    auto [handle, timer_impl] = data.m_timers.assign_new(data.m_loop, std::move(callback), timeout);
    looper_trace_info(log_module, "creating new timer: loop=%lu, handle=%lu, timeout=%lu", data.m_handle, handle, timeout.count());

    return handle;
}

void destroy_timer(const timer timer) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop_from_handle(timer);

    looper_trace_info(log_module, "destroying timer: loop=%lu, handle=%lu", data.m_handle, timer);

    const auto timer_impl = data.m_timers.release(timer);
    timer_impl->stop();
}

void start_timer(const timer timer) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop_from_handle(timer);

    looper_trace_debug(log_module, "starting timer: loop=%lu, handle=%lu", data.m_handle, timer);

    auto& timer_impl = data.m_timers[timer];
    timer_impl.start();
}

void stop_timer(const timer timer) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop_from_handle(timer);

    looper_trace_debug(log_module, "stopping timer: loop=%lu, handle=%lu", data.m_handle, timer);

    auto& timer_impl = data.m_timers[timer];
    timer_impl.stop();
}

void reset_timer(const timer timer) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop_from_handle(timer);

    looper_trace_debug(log_module, "resetting timer: loop=%lu, handle=%lu", data.m_handle, timer);

    auto& timer_impl = data.m_timers[timer];
    timer_impl.reset();
}

}
