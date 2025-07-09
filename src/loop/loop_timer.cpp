
#include "loop_timer.h"

namespace looper::impl {

#define log_module loop_log_module "_timer"

timer::timer(const looper::timer handle, loop_context* context, timer_callback&& callback, const std::chrono::milliseconds timeout)
    : m_handle(handle)
    , m_context(context)
    , m_running(false)
    , m_timeout(timeout)
    , m_callback(std::move(callback))
    , m_context_data()
{}

void timer::start() {
    std::unique_lock lock(m_context->mutex);

    if (m_timeout < min_poll_timeout) {
        throw std::runtime_error("timer timeout too small");
    }

    m_running = true;
    m_context_data.timeout = m_timeout;
    m_context_data.hit = false;
    m_context_data.next_timestamp = time_now() + m_timeout;
    m_context_data.callback = [this]()->void {
        handle_events();
    };

    m_context->timers.push_back(&m_context_data);

    looper_trace_info(log_module, "starting timer: ptr=0x%x, next_time=%lu", this, m_context_data.next_timestamp.count());

    if (m_context->timeout > m_timeout) {
        m_context->timeout = m_timeout;
    }

    signal_run(m_context);
}

void timer::stop() {
    std::unique_lock lock(m_context->mutex);

    looper_trace_info(log_module, "removing timer: ptr=0x%x", this);

    m_context->timers.remove(&m_context_data);
    reset_smallest_timeout(m_context);
}

void timer::reset() {
    std::unique_lock lock(m_context->mutex);
    if (!m_running) {
        return;
    }

    m_context_data.hit = false;
    m_context_data.next_timestamp = time_now() + m_timeout;

    looper_trace_info(log_module, "resetting timer: ptr=0x%x, next_time=%lu", this, m_context_data.next_timestamp.count());
}

void timer::handle_events() {
    std::unique_lock lock(m_context->mutex);
    invoke_func(lock, "timer_callback", m_callback, m_context->handle, m_handle);
}

}
