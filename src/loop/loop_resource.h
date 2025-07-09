#pragma once

#include "loop_internal.h"

namespace looper::impl {

class looper_resource {
public:
    explicit looper_resource(loop_context* context);
    virtual ~looper_resource();

    void attach_to_loop(os::descriptor descriptor, event_types events);
    void detach_from_loop();
    void request_events(event_types events, events_update_type type = events_update_type::override);

protected:
    virtual void handle_events(std::unique_lock<std::mutex>& lock, event_types events) = 0;

    loop_context* m_context;
private:
    looper::impl::resource m_resource;
};

}
