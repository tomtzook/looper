
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

static impl::loop_context* get_loop(loop loop) {
    auto data = g_instance.m_loops[loop];
    return data->m_context;
}

static impl::loop_context* get_loop_from_handle(handle handle) {
    handles::handle full(handle);
    return get_loop(full.parent());
}

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
    auto context = get_loop(loop);
    impl::run_once(context);
}

void run_forever(loop loop) {
    auto context = get_loop(loop);
    impl::run_forever(context);
}

// execute
future execute_on(loop loop, std::chrono::milliseconds delay, loop_callback&& callback) {
    auto context = get_loop(loop);
    auto future = impl::create_future(context, [callback](looper::loop loop, looper::future future)->void {
        callback(loop);

        auto context = get_loop(loop);
        impl::destroy_future(context, future);
    });

    impl::execute_later(context, future, delay);

    return future;
}

bool wait_for(future future, std::chrono::milliseconds timeout) {
    auto context = get_loop_from_handle(future);
    return impl::wait_for(context, future, timeout);
}

// events
event create_event(loop loop, event_callback&& callback) {
    auto context = get_loop(loop);
    return impl::create_event(context, std::move(callback));
}

void destroy_event(event event) {
    auto context = get_loop_from_handle(event);
    impl::destroy_event(context, event);
}

void set_event(event event) {
    auto context = get_loop_from_handle(event);
    impl::set_event(context, event);
}

void clear_event(event event) {
    auto context = get_loop_from_handle(event);
    impl::clear_event(context, event);
}

// timers
timer create_timer(loop loop, std::chrono::milliseconds timeout, timer_callback&& callback) {
    auto context = get_loop(loop);
    return impl::create_timer(context, timeout, std::move(callback));
}

void destroy_timer(timer timer) {
    auto context = get_loop_from_handle(timer);
    impl::destroy_timer(context, timer);
}

void start_timer(timer timer) {
    auto context = get_loop_from_handle(timer);
    impl::start_timer(context, timer);
}

void stop_timer(timer timer) {
    auto context = get_loop_from_handle(timer);
    impl::stop_timer(context, timer);
}

void reset_timer(timer timer) {
    auto context = get_loop_from_handle(timer);
    impl::reset_timer(context, timer);
}

// tcp
tcp create_tcp(loop loop) {
    auto context = get_loop(loop);
    return impl::create_tcp(context);
}

void destroy_tcp(tcp tcp) {
    auto context = get_loop_from_handle(tcp);
    impl::destroy_tcp(context, tcp);
}

void bind_tcp(tcp tcp, uint16_t port) {
    auto context = get_loop_from_handle(tcp);
    impl::bind_tcp(context, tcp, port);
}

void connect_tcp(tcp tcp, std::string_view server_address, uint16_t server_port, tcp_callback&& callback) {
    auto context = get_loop_from_handle(tcp);
    impl::connect_tcp(context, tcp, server_address, server_port, std::move(callback));
}

void start_tcp_read(tcp tcp, tcp_read_callback&& callback) {
    auto context = get_loop_from_handle(tcp);
    impl::start_tcp_read(context, tcp, std::move(callback));
}

void write_tcp(tcp tcp, std::span<const uint8_t> buffer, tcp_callback&& callback) {
    auto context = get_loop_from_handle(tcp);
    impl::write_tcp(context, tcp, buffer, std::move(callback));
}

}
