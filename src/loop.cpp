
#include <looper_trace.h>

#include "loop.h"

namespace looper {

#define log_module "loop"

static constexpr size_t max_events_for_process = 20;
static constexpr auto initial_poll_timeout = std::chrono::milliseconds(1000);
static constexpr auto min_poll_timeout = std::chrono::milliseconds(100);

loop_impl::loop_impl(loop handle, std::unique_ptr<poller>&& poller)
    : m_handle(handle)
    , m_mutex()
    , m_poller(std::move(poller))
    , m_timeout(initial_poll_timeout)
{}

void loop_impl::execute_later(execute_callback&& callback, bool wait) {
    std::unique_lock lock(m_mutex);

    execute_request request{};
    request.callback = std::move(callback);
    m_execute_requests.push_back(request);

    // todo: signal run

    if (wait) {
        // todo: wait for run
    }
}

void loop_impl::run_once() {
    std::unique_lock lock(m_mutex);

    lock.unlock();
    auto result = m_poller->poll(max_events_for_process, m_timeout);
    lock.lock();

    execute_requests(lock);
}

void loop_impl::execute_requests(std::unique_lock<std::mutex>& lock) {
    while (!m_execute_requests.empty()) {
        auto& request = m_execute_requests.front();

        lock.unlock();
        try {
            request.callback(m_handle);
        } catch (const std::exception& e) {
            looper_trace_error(log_module, "Error in request callback: what=%s", e.what());
        } catch (...) {
            looper_trace_error(log_module, "Error in request callback: unknown");
        }
        lock.lock();

        m_execute_requests.pop_front();
    }
}

}
