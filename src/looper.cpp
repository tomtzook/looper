
#include <looper.h>
#include <looper_tcp.h>

#include "util/handles.h"
#include "loop.h"

namespace looper {

#define log_module "looper"

struct loop_data {
    explicit loop_data(loop loop)
        : m_context(impl::create_loop(loop))
        , m_destroyed(false)
    {}
    ~loop_data() {
        if (!m_destroyed) {
            impl::destroy_loop(m_context);
            m_destroyed = true;
        }
    }

    impl::loop_context* m_context;
    bool m_destroyed;
};

struct looper_data {
    looper_data()
        : m_loops(0, handles::type_loop)
    {}

    handles::handle_table<loop_data, 8> m_loops;
};

looper_data g_instance;

loop create() {
    auto [handle, data] = g_instance.m_loops.allocate_new();
    return handle;
}

void destroy(loop loop) {
    auto data = g_instance.m_loops.release(loop);
    impl::destroy_loop(data->m_context);
    data->m_destroyed = true;
}

void run_once(loop loop) {
    auto data = g_instance.m_loops[loop];
    impl::run_once(data->m_context);
}

void run_forever(loop loop) {
    auto data = g_instance.m_loops[loop];
    impl::run_forever(data->m_context);
}

// execute
future execute_on(loop loop, std::chrono::milliseconds delay, loop_callback&& callback) {
    auto data = g_instance.m_loops[loop];
    auto future = impl::create_future(data->m_context, [callback](looper::loop loop, looper::future future)->void {
        callback(loop);

        auto data = g_instance.m_loops[loop];
        impl::destroy_future(data->m_context, future);
    });

    impl::execute_later(data->m_context, future, delay);

    return future;
}

bool wait_for(loop loop, future future, std::chrono::milliseconds timeout) {
    auto data = g_instance.m_loops[loop];
    return impl::wait_for(data->m_context, future, timeout);
}

// events
event create_event(loop loop, event_callback&& callback) {
    auto data = g_instance.m_loops[loop];
    return impl::create_event(data->m_context, std::move(callback));
}

void destroy_event(loop loop, event event) {
    auto data = g_instance.m_loops[loop];
    impl::destroy_event(data->m_context, event);
}

void set_event(loop loop, event event) {
    auto data = g_instance.m_loops[loop];
    impl::set_event(data->m_context, event);
}

void clear_event(loop loop, event event) {
    auto data = g_instance.m_loops[loop];
    impl::clear_event(data->m_context, event);
}

// timers
timer create_timer(loop loop, std::chrono::milliseconds timeout, timer_callback&& callback) {
    auto data = g_instance.m_loops[loop];
    return impl::create_timer(data->m_context, timeout, std::move(callback));
}

void destroy_timer(loop loop, timer timer) {
    auto data = g_instance.m_loops[loop];
    impl::destroy_timer(data->m_context, timer);
}

void start_timer(loop loop, timer timer) {
    auto data = g_instance.m_loops[loop];
    impl::start_timer(data->m_context, timer);
}

void stop_timer(loop loop, timer timer) {
    auto data = g_instance.m_loops[loop];
    impl::stop_timer(data->m_context, timer);
}

void reset_timer(loop loop, timer timer) {
    auto data = g_instance.m_loops[loop];
    impl::reset_timer(data->m_context, timer);
}

// tcp
tcp create_tcp(loop loop) {
    auto data = g_instance.m_loops[loop];
    return impl::create_tcp(data->m_context);
}

void destroy_tcp(loop loop, tcp tcp) {
    auto data = g_instance.m_loops[loop];
    impl::destroy_tcp(data->m_context, tcp);
}

void bind_tcp(loop loop, tcp tcp, uint16_t port) {
    auto data = g_instance.m_loops[loop];
    impl::bind_tcp(data->m_context, tcp, port);
}

void connect_tcp(loop loop, tcp tcp, std::string_view server_address, uint16_t server_port, tcp_callback&& callback) {
    auto data = g_instance.m_loops[loop];
    impl::connect_tcp(data->m_context, tcp, server_address, server_port, std::move(callback));
}

void start_tcp_read(loop loop, tcp tcp, tcp_read_callback&& callback) {
    auto data = g_instance.m_loops[loop];
    impl::start_tcp_read(data->m_context, tcp, std::move(callback));
}

void write_tcp(loop loop, tcp tcp, std::span<const uint8_t> buffer, tcp_callback&& callback) {
    auto data = g_instance.m_loops[loop];
    impl::write_tcp(data->m_context, tcp, buffer, std::move(callback));
}

}
