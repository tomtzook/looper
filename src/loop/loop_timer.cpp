
#include "loop_timer.h"

namespace looper::impl {

#define log_module loop_log_module "_timer"

timer::timer(const looper::timer handle, loop_ptr loop, timer_callback&& callback, const std::chrono::milliseconds timeout)
    : m_handle(handle)
    , m_loop(std::move(loop))
    , m_running(false)
    , m_timeout(timeout)
    , m_callback(std::move(callback))
    , m_loop_data()
{}

looper::error timer::start() {
    auto lock = m_loop->lock_loop();

    if (m_timeout < min_poll_timeout) {
        return error_timeout_too_small;
    }

    if (m_running) {
        return error_already_running;
    }

    m_loop_data.timeout = m_timeout;
    m_loop_data.hit = false;
    m_loop_data.next_timestamp = time_now() + m_timeout;
    m_loop_data.callback = [this]()->void {
        handle_events();
    };

    m_loop->add_timer(&m_loop_data);
    m_running = true;

    looper_trace_info(log_module, "starting timer: handle=%lu, next_time=%lu", m_handle, m_loop_data.next_timestamp.count());

    m_loop->set_timeout_if_smaller(m_timeout);
    m_loop->signal_run();

    return error_success;
}

void timer::stop() {
    auto lock = m_loop->lock_loop();

    if (!m_running) {
        return;
    }

    looper_trace_info(log_module, "removing timer: handle=%lu", m_handle);

    m_loop->remove_timer(&m_loop_data);
    m_running = false;

    m_loop->reset_smallest_timeout();
}

void timer::reset() {
    auto lock = m_loop->lock_loop();
    if (!m_running) {
        return;
    }

    m_loop_data.hit = false;
    m_loop_data.next_timestamp = time_now() + m_timeout;

    looper_trace_info(log_module, "resetting timer: handle=%lu, next_time=%lu", m_handle, m_loop_data.next_timestamp.count());
}

void timer::handle_events() const {
    auto lock = m_loop->lock_loop();
    invoke_func(lock, "timer_callback", m_callback, m_handle);
}

}
