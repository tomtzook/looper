#include "looper_base.h"

namespace looper {

#define log_module looper_log_module

looper_data& get_global_loop_data() {
    static looper_data g_instance{};
    return g_instance;
}

static void run_loop_forever(loop loop) {
    while (true) {
        std::unique_lock lock(get_global_loop_data().m_mutex);
        auto data_opt = try_get_loop(loop);
        if (!data_opt) {
            break;
        }

        auto* data = data_opt.value();
        lock.unlock();

        bool finished = impl::run_once(data->m_context);
        if (finished) {
            break;
        }
    }
}

static void thread_main(loop loop) {
    run_loop_forever(loop);
}

static void future_loop_callback(impl::future_data* future) {
    auto loop = get_loop_handle(future->handle);

    looper_trace_debug(log_module, "future callback called: loop=%lu, handle=%lu", loop, future->handle);
    invoke_func_nolock("future_user_callback", future->user_callback, loop, future->handle);

    future->exec_finished.notify_all();
}

static void event_loop_callback(impl::event_data* event) {
    auto loop = get_loop_handle(event->handle);

    looper_trace_debug(log_module, "event callback called: loop=%lu, handle=%lu", loop, event->handle);
    invoke_func_nolock("event_user_callback", event->user_callback, loop, event->handle);
}

static void timer_loop_callback(impl::timer_data* timer) {
    auto loop = get_loop_handle(timer->handle);

    looper_trace_debug(log_module, "timer callback called: loop=%lu, handle=%lu", loop, timer->handle);
    invoke_func_nolock("timer_user_callback", timer->user_callback, loop, timer->handle);
}

static future create_future_internal(loop loop, future_callback&& callback) {
    auto& data = get_loop(loop);

    auto [handle, future_data] = data.m_futures.allocate_new();
    future_data->user_callback = std::move(callback);
    future_data->from_loop_callback = future_loop_callback;

    looper_trace_info(log_module, "creating future: loop=%lu, handle=%lu", loop, handle);

    impl::add_future(data.m_context, future_data.get());

    data.m_futures.assign(handle, std::move(future_data));

    return handle;
}

static void destroy_future_internal(future future) {
    auto& data = get_loop_from_handle(future);

    looper_trace_info(log_module, "destroying future: loop=%lu, handle=%lu", data.m_handle, future);

    auto future_data = data.m_futures.release(future);
    impl::remove_future(data.m_context, future_data.get());
}

static void execute_future_internal(future future, std::chrono::milliseconds delay) {
    auto& data = get_loop_from_handle(future);

    auto& future_data = data.m_futures[future];
    if (!future_data.finished) {
        throw std::runtime_error("future already queued for execution");
    }

    looper_trace_info(log_module, "requesting future execution: loop=%lu, handle=%lu, delay=%lu", data.m_handle, future, delay.count());

    future_data.delay = delay;
    impl::exec_future(data.m_context, &future_data);
}

static bool wait_for_future_internal(std::unique_lock<std::mutex>& lock, future future, std::chrono::milliseconds timeout) {
    auto& data = get_loop_from_handle(future);

    auto& future_data = data.m_futures[future];
    if (future_data.finished) {
        looper_trace_debug(log_module, "future already finished, not waiting: loop=%lu, handle=%lu", data.m_handle, future);
        return false;
    }

    looper_trace_info(log_module, "waiting on future: loop=%lu, handle=%lu, timeout=%lu", data.m_handle, future, timeout.count());

    return !future_data.exec_finished.wait_for(lock, timeout, [future]()->bool {
        auto& data = get_loop_from_handle(future);

        if (!data.m_futures.has(future)) {
            return true;
        }

        auto& future_data = data.m_futures[future];
        return future_data.finished;
    });
}

loop create() {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto [handle, data] = get_global_loop_data().m_loops.allocate_new();
    get_global_loop_data().m_loops.assign(handle, std::move(data));

    looper_trace_info(log_module, "created new loop: handle=%lu", handle);

    return handle;
}

void destroy(loop loop) {
    std::unique_lock lock(get_global_loop_data().m_mutex);
    auto& data = get_loop(loop);
    data.m_closing = true;

    looper_trace_info(log_module, "destroying loop: handle=%lu", loop);

    auto thread = std::move(data.m_thread);
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

void run_once(loop loop) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop(loop);
    if (data.m_thread) {
        throw std::runtime_error("loop running in thread");
    }

    looper_trace_debug(log_module, "running loop once: handle=%lu", loop);

    lock.unlock();
    impl::run_once(data.m_context);
}

void run_forever(loop loop) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop(loop);
    if (data.m_thread) {
        throw std::runtime_error("loop running in thread");
    }

    looper_trace_info(log_module, "running loop forever: handle=%lu", loop);

    lock.unlock();
    run_loop_forever(loop);
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
future create_future(loop loop, future_callback&& callback) {
    std::unique_lock lock(get_global_loop_data().m_mutex);
    return create_future_internal(loop, std::move(callback));
}

void destroy_future(future future) {
    std::unique_lock lock(get_global_loop_data().m_mutex);
    destroy_future_internal(future);
}

void execute_once(future future, std::chrono::milliseconds delay) {
    std::unique_lock lock(get_global_loop_data().m_mutex);
    execute_future_internal(future, delay);
}

bool wait_for(future future, std::chrono::milliseconds timeout) {
    std::unique_lock lock(get_global_loop_data().m_mutex);
    return wait_for_future_internal(lock, future, timeout);
}

void execute_later(loop loop, loop_callback&& callback) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto future = create_future_internal(loop, [callback](looper::loop loop, looper::future future)->void {
        std::unique_lock lock(get_global_loop_data().m_mutex);
        destroy_future_internal(future);

        invoke_func(lock, "future_singleuse_callback", callback, loop);
    });
    execute_future_internal(future, no_delay);
}

bool execute_later_and_wait(loop loop, loop_callback&& callback, std::chrono::milliseconds timeout) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto future = create_future_internal(loop, [callback](looper::loop loop, looper::future future)->void {
        std::unique_lock lock(get_global_loop_data().m_mutex);
        destroy_future_internal(future);

        invoke_func(lock, "future_singleuse_callback", callback, loop);
    });
    execute_future_internal(future, no_delay);

    return wait_for_future_internal(lock, future, timeout);
}

// events
event create_event(loop loop, event_callback&& callback) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop(loop);

    auto [handle, event_data] = data.m_events.allocate_new();
    event_data->user_callback = std::move(callback);
    event_data->event_obj = os::make_event();
    event_data->from_loop_callback = event_loop_callback;

    looper_trace_info(log_module, "creating new event: loop=%lu, handle=%lu", loop, handle);

    impl::add_event(data.m_context, event_data.get());

    data.m_events.assign(handle, std::move(event_data));

    return handle;
}

void destroy_event(event event) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop_from_handle(event);

    looper_trace_info(log_module, "destroying event: loop=%lu, handle=%lu", data.m_handle, event);

    auto event_data = data.m_events.release(event);
    impl::remove_event(data.m_context, event_data.get());
}

void set_event(event event) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop_from_handle(event);

    looper_trace_debug(log_module, "setting event: loop=%lu, handle=%lu", data.m_handle, event);

    auto& event_data = data.m_events[event];
    // todo: perhaps reports errors via callback only
    OS_CHECK_THROW(os::event::set(event_data.event_obj.get()));
}

void clear_event(event event) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop_from_handle(event);

    looper_trace_debug(log_module, "clearing event: loop=%lu, handle=%lu", data.m_handle, event);

    auto& event_data = data.m_events[event];
    OS_CHECK_THROW(os::event::clear(event_data.event_obj.get()));
}

// timers
timer create_timer(loop loop, std::chrono::milliseconds timeout, timer_callback&& callback) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop(loop);

    auto [handle, timer_data] = data.m_timers.assign_new();
    timer_data.user_callback = std::move(callback);
    timer_data.timeout = timeout;
    timer_data.from_loop_callback = timer_loop_callback;

    looper_trace_info(log_module, "creating new timer: loop=%lu, handle=%lu, timeout=%lu", data.m_handle, handle, timeout.count());

    return handle;
}

void destroy_timer(timer timer) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop_from_handle(timer);

    looper_trace_info(log_module, "destroying timer: loop=%lu, handle=%lu", data.m_handle, timer);

    auto timer_data = data.m_timers.release(timer);
    if (timer_data->running) {
        impl::remove_timer(data.m_context, timer_data.get());
    }
}

void start_timer(timer timer) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop_from_handle(timer);

    auto& timer_data = data.m_timers[timer];
    if (!timer_data.running) {
        looper_trace_debug(log_module, "starting timer: loop=%lu, handle=%lu", data.m_handle, timer);

        impl::add_timer(data.m_context, &timer_data);
    }
}

void stop_timer(timer timer) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop_from_handle(timer);

    auto& timer_data = data.m_timers[timer];
    if (timer_data.running) {
        looper_trace_debug(log_module, "stopping timer: loop=%lu, handle=%lu", data.m_handle, timer);

        impl::remove_timer(data.m_context, &timer_data);
    }
}

void reset_timer(timer timer) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop_from_handle(timer);

    auto& timer_data = data.m_timers[timer];
    if (timer_data.running) {
        looper_trace_debug(log_module, "resetting timer: loop=%lu, handle=%lu", data.m_handle, timer);

        impl::reset_timer(data.m_context, &timer_data);
    }
}

}
