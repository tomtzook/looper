#pragma once

#include "os/meta.h"
#include "loop_resource.h"

namespace looper::impl {

class event final {
public:
    event(looper::event handle, const loop_ptr& loop, event_callback&& callback);

    void set();
    void clear();

private:
    void handle_events(std::unique_lock<std::mutex>& lock, const loop_resource::control& control, event_types events) const;

    looper::event m_handle;
    os::event_ptr m_event_obj;
    loop_resource m_resource;
    event_callback m_callback;
};

}
