
#include <looper.h>

#include "instance.h"

namespace looper {

#define log_module "looper"

loop create() {
    auto [handle, data] = g_instance.m_loops.allocate_new();
    return handle;
}

void destroy(loop loop) {
    auto data = g_instance.m_loops.release(loop);
}

void run_once(loop loop) {
    auto data = g_instance.m_loops[loop];
    data->run();
}

void run_forever(loop loop) {
    auto data = g_instance.m_loops[loop];
    // todo: need to stop at some point, when
    // todo: what if someone calls destroy
}

void execute_on(loop loop, execute_callback&& callback) {
    auto data = g_instance.m_loops[loop];
    data->execute_later(std::move(callback), false);
}

void execute_on_and_wait(loop loop, execute_callback&& callback) {
    auto data = g_instance.m_loops[loop];
    data->execute_later(std::move(callback), true);
}

event create_event(loop loop, event_callback&& callback) {
    auto data = g_instance.m_loops[loop];

    auto event = create_event();
    event->clear();

    return data->add_event(event, std::move(callback));
}

void destroy_event(loop loop, event event) {
    auto data = g_instance.m_loops[loop];
    data->remove_event(event);
}

void set_event(loop loop, event event) {
    auto data = g_instance.m_loops[loop];
    auto event_data = data->get_event(event);
    event_data->event->set();
}

void clear_event(loop loop, event event) {
    auto data = g_instance.m_loops[loop];
    auto event_data = data->get_event(event);
    event_data->event->clear();
}

}
