#pragma once

#include "os/factory.h"
#include "loop_resource.h"

namespace looper::impl {

class event final : protected looper_resource {
public:
    event(looper::event handle, loop_context* context, event_callback&& callback);

    void set();
    void clear();

protected:
    void handle_events(std::unique_lock<std::mutex>& lock, event_types events) override;

private:
    looper::event m_handle;
    os::event_ptr m_event_obj;
    event_callback m_callback;
};

}
