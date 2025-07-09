
#include "loop_future.h"

namespace looper::impl {

#define log_module loop_log_module "_future"

future::future(const looper::future handle, loop_context* context, future_callback&& callback)
    : m_handle(handle)
    , m_context(context)
    , m_callback(std::move(callback))
    , m_exec_finished()
    , m_context_data()
{}

future::~future() {
    std::unique_lock lock(m_context->mutex);
    m_context->futures.remove(&m_context_data);
}

void future::execute(const std::chrono::milliseconds delay) {
    std::unique_lock lock(m_context->mutex);

    if (!m_context_data.finished) {
        throw std::runtime_error("future already queued for execution");
    }

    m_context_data.finished = false;
    m_context_data.execute_time = time_now() + delay;
    m_context_data.callback = [this]()->void {
        handle_events();
    };

    looper_trace_info(log_module, "queueing future: ptr=0x%x, run_at=%lu", this, m_context_data.execute_time.count());

    m_context->futures.push_back(&m_context_data);
    if (delay.count() < 1) {
        signal_run(m_context);
    }
}

bool future::wait_for(std::unique_lock<std::mutex>& lock, const std::chrono::milliseconds timeout) {
    if (m_context_data.finished) {
        looper_trace_debug(log_module, "future already finished, not waiting: loop=%lu, handle=%lu", m_context->handle, m_handle);
        return false;
    }

    looper_trace_info(log_module, "waiting on future: loop=%lu, handle=%lu, timeout=%lu", m_context->handle, m_handle, timeout.count());

    return !m_exec_finished.wait_for(lock, timeout, [this]()->bool {
        return m_context_data.finished;
    });
}


void future::handle_events() {
    std::unique_lock lock(m_context->mutex);
    invoke_func(lock, "future_callback", m_callback, m_context->handle, m_handle);
    m_context->futures.remove(&m_context_data);
    m_exec_finished.notify_all();
}

}
