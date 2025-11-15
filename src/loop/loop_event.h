#pragma once

#include "loop_resource.h"
#include "os/os.h"

namespace looper::impl {

class event final {
public:
    event(looper::event handle, const loop_ptr& loop, event_callback&& callback);

    looper::error set();
    looper::error clear();

private:
    void handle_events(std::unique_lock<std::mutex>& lock, const loop_resource::control& control, event_type events) const;

    looper::event m_handle;
    os::event m_event_obj;
    loop_resource m_resource;
    event_callback m_callback;
};

}
