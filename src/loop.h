#pragma once

#include <memory>
#include <deque>
#include <mutex>

#include <looper_types.h>

#include "util/handles.h"
#include "poll.h"

namespace looper {

class loop_impl {
public:
    loop_impl(loop handle, std::unique_ptr<poller>&& poller);

    void execute_later(execute_callback&& callback, bool wait);

    void run_once();

private:
    struct execute_request {
        execute_callback callback;
    };

    void execute_requests(std::unique_lock<std::mutex>& lock);

    loop m_handle;
    std::mutex m_mutex;
    std::unique_ptr<poller> m_poller;
    std::chrono::milliseconds m_timeout;

    std::deque<execute_request> m_execute_requests;
};

}
