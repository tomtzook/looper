#pragma once

#include "loop.h"

namespace looper::impl {

class future final {
public:
    future(looper::future handle, loop_ptr loop, future_callback&& callback);
    ~future();

    looper::error execute(std::chrono::milliseconds delay);
    bool wait_for(std::unique_lock<std::mutex>& lock, std::chrono::milliseconds timeout);
private:
    void handle_events();

    looper::future m_handle;
    loop_ptr m_loop;

    future_callback m_callback;
    std::condition_variable m_exec_finished;

    future_data m_loop_data;
};

}
