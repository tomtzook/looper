#pragma once

#include "loop_internal.h"

namespace looper::impl {

class future final {
public:
    future(looper::future handle, loop_context* context, future_callback&& callback);
    ~future();

    void execute(std::chrono::milliseconds delay);
    bool wait_for(std::unique_lock<std::mutex>& lock, std::chrono::milliseconds timeout);
private:
    void handle_events();

    looper::future m_handle;
    loop_context* m_context;

    future_callback m_callback;
    std::condition_variable m_exec_finished;

    future_data m_context_data;
};

}
