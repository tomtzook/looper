#pragma once

#include "loop.h"

namespace looper::impl {

class timer final {
public:
    timer(looper::timer handle, loop_ptr loop, timer_callback&& callback, std::chrono::milliseconds timeout);

    void start();
    void stop();
    void reset();

private:
    void handle_events();

    looper::timer m_handle;
    loop_ptr m_loop;

    bool m_running;
    std::chrono::milliseconds m_timeout;
    timer_callback m_callback;

    timer_data m_loop_data;
};

}
