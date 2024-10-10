#pragma once

#include <unordered_map>
#include <optional>

#include <looper.h>

#include "util/handles.h"
#include "factory.h"
#include "loop.h"

namespace looper {

struct event_handle_data {
public:
    explicit event_handle_data(handle handle)
        : resource(empty_handle)
        , event()
        , callback()
    {}

    looper::resource resource;
    std::shared_ptr<os::event> event;
    event_callback callback;
};

struct resource_ptr {
    handles::handle_types type;
    looper::handle handle;
};

struct loop_data {
public:
    explicit loop_data(handle handle);

    event_handle_data* get_event(event event);

    std::optional<handle> get_handle_from_resource(resource resource, handles::handle_types type);

    event add_event(std::shared_ptr<os::event> event, event_callback&& callback);
    void remove_event(event event);

    void execute_later(execute_callback&& callback, bool wait);
    void run();

private:
    resource add_resource(handle handle, std::shared_ptr<os::resource> resource, event_types events);
    void remove_resource(resource resource);

    loop_impl m_loop;

    handles::handle_table<event_handle_data, 64> m_events;

    std::unordered_map<resource, resource_ptr> m_resource_ptrs;
    resource_callback m_resource_callbacks[handles::type_max];
};

struct looper_data {
    looper_data()
        : m_loops(0, handles::type_loop)
    {}

    handles::handle_table<loop_data, 8> m_loops;
};

extern looper_data g_instance;

}
