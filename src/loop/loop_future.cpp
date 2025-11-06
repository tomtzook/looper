
#include "loop_future.h"

#include <utility>

namespace looper::impl {

#define log_module loop_log_module "_future"

future::future(const looper::future handle, loop_ptr loop, future_callback&& callback)
    : m_handle(handle)
    , m_loop(std::move(loop))
    , m_callback(std::move(callback))
    , m_exec_finished()
    , m_loop_data()
{}

future::~future() {
    auto lock = m_loop->lock_loop();
    m_loop->remove_future(&m_loop_data);
}

void future::execute(const std::chrono::milliseconds delay) {
    auto lock = m_loop->lock_loop();

    if (!m_loop_data.finished) {
        throw std::runtime_error("future already queued for execution");
    }

    m_loop_data.finished = false;
    m_loop_data.execute_time = time_now() + delay;
    m_loop_data.callback = [this]()->void {
        handle_events();
    };

    looper_trace_info(log_module, "queueing future: handle=%lu, run_at=%lu", m_handle, m_loop_data.execute_time.count());

    m_loop->add_future(&m_loop_data);
    if (delay.count() < 1) {
        m_loop->signal_run();
    }
}

bool future::wait_for(std::unique_lock<std::mutex>& lock, const std::chrono::milliseconds timeout) {
    if (m_loop_data.finished) {
        looper_trace_debug(log_module, "future already finished, not waiting: loop=%lu, handle=%lu", m_loop->handle(), m_handle);
        return false;
    }

    looper_trace_info(log_module, "waiting on future: loop=%lu, handle=%lu, timeout=%lu", m_loop->handle(), m_handle, timeout.count());

    return !m_exec_finished.wait_for(lock, timeout, [this]()->bool {
        return m_loop_data.finished;
    });
}

void future::handle_events() {
    auto lock = m_loop->lock_loop();

    m_loop->remove_future(&m_loop_data);
    m_exec_finished.notify_all();

    invoke_func(lock, "future_callback", m_callback, m_handle);
}

}
