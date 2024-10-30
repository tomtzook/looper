#include <optional>
#include <thread>

#include <looper.h>
#include <looper_tcp.h>

#include "util/handles.h"
#include "util/util.h"
#include "os/factory.h"
#include "loop.h"

namespace looper {

#define log_module "looper"

struct loop_data {
    static constexpr size_t handle_counts_per_type = 64;

    explicit loop_data(loop handle)
        : m_handle(handle)
        , m_context(impl::create_loop())
        , m_closing(false)
        , m_destroyed(false)
        , m_thread(nullptr)
        , m_events(handles::handle{handle}.index(), handles::type_event)
        , m_timers(handles::handle{handle}.index(), handles::type_timer)
        , m_futures(handles::handle{handle}.index(), handles::type_future)
        , m_tcps(handles::handle{handle}.index(), handles::type_tcp)
        , m_tcp_servers(handles::handle{handle}.index(), handles::type_tcp_server)
    {}
    ~loop_data() {
        if (!m_destroyed) {
            impl::destroy_loop(m_context);
            m_destroyed = true;
        }
    }

    loop m_handle;
    impl::loop_context* m_context;
    bool m_closing;
    bool m_destroyed;

    std::unique_ptr<std::thread> m_thread;
    handles::handle_table<impl::event_data, handle_counts_per_type> m_events;
    handles::handle_table<impl::timer_data, handle_counts_per_type> m_timers;
    handles::handle_table<impl::future_data, handle_counts_per_type> m_futures;
    handles::handle_table<impl::tcp_data, handle_counts_per_type> m_tcps;
    handles::handle_table<impl::tcp_server_data, handle_counts_per_type> m_tcp_servers;
};

struct looper_data {
    looper_data()
        : m_mutex()
        , m_loops(0, handles::type_loop)
    {}

    std::mutex m_mutex;
    handles::handle_table<loop_data, 8> m_loops;
};

// todo: we use this mutex everywhere, could be problematic, limit use. perhaps remove lock from loop layer, how?
//  could use some lock-less mechanisms, or spinlocks
static looper_data& get_global_loop_data() {
    static looper_data g_instance;
    return g_instance;
}

static inline std::optional<loop_data*> try_get_loop(loop loop) {
    if (!get_global_loop_data().m_loops.has(loop)) {
        return std::nullopt;
    }

    auto& data = get_global_loop_data().m_loops[loop];
    if (data.m_closing) {
        return std::nullopt;
    }

    return {&data};
}

static inline loop_data& get_loop(loop loop) {
    auto& data = get_global_loop_data().m_loops[loop];
    if (data.m_closing) {
        throw loop_closing_exception(loop);
    }

    return data;
}

static inline loop get_loop_handle(handle handle) {
    handles::handle full(handle);
    handles::handle loop(0, handles::type_loop, full.parent());
    return loop.raw();
}

static inline loop_data& get_loop_from_handle(handle handle) {
    auto loop_handle = get_loop_handle(handle);
    return get_loop(loop_handle);
}

static void run_loop_forever(loop loop) {
    while (true) {
        std::unique_lock lock(g_instance->m_mutex);
        auto data_opt = try_get_loop(loop);
        if (!data_opt) {
            break;
        }

        auto* data = data_opt.value();
        lock.unlock();

        bool finished = impl::run_once(data->m_context);
        if (finished) {
            break;
        }
    }
}

static void thread_main(loop loop) {
    run_loop_forever(loop);
}

static void future_loop_callback(impl::future_data* future) {
    auto loop = get_loop_handle(future->handle);
    invoke_func_nolock("future_user_callback", future->user_callback, loop, future->handle);

    future->exec_finished.notify_all();
}

static void event_loop_callback(impl::event_data* event) {
    auto loop = get_loop_handle(event->handle);
    invoke_func_nolock("event_user_callback", event->user_callback, loop, event->handle);
}

static void timer_loop_callback(impl::timer_data* timer) {
    auto loop = get_loop_handle(timer->handle);
    invoke_func_nolock("timer_user_callback", timer->user_callback, loop, timer->handle);
}

static void tcp_loop_callback(impl::tcp_data* tcp, impl::tcp_data::cause cause, impl::tcp_data::cause_data cause_data, looper::error error) {
    auto loop = get_loop_handle(tcp->handle);

    switch (cause) {
        case impl::tcp_data::cause::connect:
            invoke_func_nolock("tcp_connect_callback", tcp->connect_callback, loop, tcp->handle, error);
            break;
        case impl::tcp_data::cause::write_finished:
            invoke_func_nolock("tcp_write_callback", tcp->write_callback, loop, tcp->handle, error);
            break;
        case impl::tcp_data::cause::read:
            invoke_func_nolock("tcp_read_callback", tcp->read_callback, loop, tcp->handle, cause_data.read.data, error);
            break;
        case impl::tcp_data::cause::error:
            // todo: how to pass to user?
            std::abort();
            break;
    }
}

static void tcp_server_loop_callback(impl::tcp_server_data* tcp) {
    auto loop = get_loop_handle(tcp->handle);
    invoke_func_nolock("tcp_accept_user_callback", tcp->connect_callback, loop, tcp->handle);
}

void initialize() {
    get_global_loop_data();
}

loop create() {
    std::unique_lock lock(g_instance->m_mutex);

    auto [handle, data] = get_global_loop_data().m_loops.allocate_new();
    get_global_loop_data().m_loops.assign(handle, std::move(data));

    return handle;
}

void destroy(loop loop) {
    std::unique_lock lock(g_instance->m_mutex);
    auto& data = get_loop(loop);
    data.m_closing = true;

    lock.unlock();

    if (data.m_thread && data.m_thread->joinable()) {
        data.m_thread->join();
    }

    impl::destroy_loop(data.m_context);
    lock.lock();

    auto released_data = get_global_loop_data().m_loops.release(loop);
    released_data->m_destroyed = true;
}

void run_once(loop loop) {
    std::unique_lock lock(g_instance->m_mutex);

    auto& data = get_loop(loop);
    if (data.m_thread) {
        throw std::runtime_error("loop running in thread");
    }

    lock.unlock();
    impl::run_once(data.m_context);
}

void run_forever(loop loop) {
    std::unique_lock lock(g_instance->m_mutex);

    auto& data = get_loop(loop);
    if (data.m_thread) {
        throw std::runtime_error("loop running in thread");
    }

    lock.unlock();
    run_loop_forever(loop);
}

void exec_in_thread(loop loop) {
    std::unique_lock lock(g_instance->m_mutex);

    auto& data = get_loop(loop);
    if (data.m_thread) {
        return;
    }

    data.m_thread = std::make_unique<std::thread>(&thread_main, loop);
}

// execute
future create_future(loop loop, future_callback&& callback) {
    std::unique_lock lock(g_instance->m_mutex);

    auto& data = get_loop(loop);

    auto [handle, future_data] = data.m_futures.allocate_new();
    future_data->user_callback = std::move(callback);
    future_data->from_loop_callback = future_loop_callback;

    impl::add_future(data.m_context, future_data.get());

    data.m_futures.assign(handle, std::move(future_data));

    return handle;
}

void destroy_future(future future) {
    std::unique_lock lock(g_instance->m_mutex);

    auto& data = get_loop_from_handle(future);

    auto future_data = data.m_futures.release(future);
    impl::remove_future(data.m_context, future_data.get());
}

void execute_once(future future, std::chrono::milliseconds delay) {
    std::unique_lock lock(g_instance->m_mutex);

    auto& data = get_loop_from_handle(future);

    auto& future_data = data.m_futures[future];
    if (!future_data.finished) {
        throw std::runtime_error("future already queued for execution");
    }

    future_data.delay = delay;
    impl::exec_future(data.m_context, &future_data);
}

bool wait_for(future future, std::chrono::milliseconds timeout) {
    std::unique_lock lock(g_instance->m_mutex);

    auto& data = get_loop_from_handle(future);

    auto& future_data = data.m_futures[future];
    if (future_data.finished) {
        return false;
    }

    return !future_data.exec_finished.wait_for(lock, timeout, [future]()->bool {
        auto& data = get_loop_from_handle(future);

        if (!data.m_futures.has(future)) {
            return true;
        }

        auto& future_data = data.m_futures[future];
        return future_data.finished;
    });
}

// events
event create_event(loop loop, event_callback&& callback) {
    std::unique_lock lock(g_instance->m_mutex);

    auto& data = get_loop(loop);

    auto [handle, event_data] = data.m_events.allocate_new();
    event_data->user_callback = std::move(callback);
    event_data->event_obj = os::create_event();
    event_data->from_loop_callback = event_loop_callback;

    impl::add_event(data.m_context, event_data.get());

    data.m_events.assign(handle, std::move(event_data));

    return handle;
}

void destroy_event(event event) {
    std::unique_lock lock(g_instance->m_mutex);

    auto& data = get_loop_from_handle(event);
    
    auto event_data = data.m_events.release(event);
    impl::remove_event(data.m_context, event_data.get());
}

void set_event(event event) {
    std::unique_lock lock(g_instance->m_mutex);

    auto& data = get_loop_from_handle(event);
    
    auto& event_data = data.m_events[event];
    event_data.event_obj->set();
}

void clear_event(event event) {
    std::unique_lock lock(g_instance->m_mutex);

    auto& data = get_loop_from_handle(event);

    auto& event_data = data.m_events[event];
    event_data.event_obj->clear();
}

// timers
timer create_timer(loop loop, std::chrono::milliseconds timeout, timer_callback&& callback) {
    std::unique_lock lock(g_instance->m_mutex);

    auto& data = get_loop(loop);

    auto [handle, timer_data] = data.m_timers.assign_new();
    timer_data.user_callback = std::move(callback);
    timer_data.timeout = timeout;
    timer_data.from_loop_callback = timer_loop_callback;

    return handle;
}

void destroy_timer(timer timer) {
    std::unique_lock lock(g_instance->m_mutex);

    auto& data = get_loop_from_handle(timer);

    auto timer_data = data.m_timers.release(timer);
    if (timer_data->running) {
        impl::remove_timer(data.m_context, timer_data.get());
    }
}

void start_timer(timer timer) {
    std::unique_lock lock(g_instance->m_mutex);

    auto& data = get_loop_from_handle(timer);

    auto& timer_data = data.m_timers[timer];
    if (!timer_data.running) {
        impl::add_timer(data.m_context, &timer_data);
    }
}

void stop_timer(timer timer) {
    std::unique_lock lock(g_instance->m_mutex);

    auto& data = get_loop_from_handle(timer);

    auto& timer_data = data.m_timers[timer];
    if (timer_data.running) {
        impl::remove_timer(data.m_context, &timer_data);
    }
}

void reset_timer(timer timer) {
    std::unique_lock lock(g_instance->m_mutex);

    auto& data = get_loop_from_handle(timer);

    auto& timer_data = data.m_timers[timer];
    if (timer_data.running) {
        impl::reset_timer(data.m_context, &timer_data);
    }
}

// tcp
tcp create_tcp(loop loop) {
    std::unique_lock lock(g_instance->m_mutex);

    auto& data = get_loop(loop);

    auto [handle, tcp_data] = data.m_tcps.allocate_new();
    tcp_data->callback = tcp_loop_callback;
    tcp_data->socket_obj = os::create_tcp_socket();
    tcp_data->state = impl::tcp_data::state::open;

    impl::add_tcp(data.m_context, tcp_data.get());

    data.m_tcps.assign(handle, std::move(tcp_data));

    return handle;
}

void destroy_tcp(tcp tcp) {
    std::unique_lock lock(g_instance->m_mutex);

    auto& data = get_loop_from_handle(tcp);

    auto tcp_data = data.m_tcps.release(tcp);
    impl::remove_tcp(data.m_context, tcp_data.get());

    if (tcp_data->socket_obj) {
        tcp_data->socket_obj->close();
        tcp_data->socket_obj.reset();
    }
}

void bind_tcp(tcp tcp, uint16_t port) {
    std::unique_lock lock(g_instance->m_mutex);

    auto& data = get_loop_from_handle(tcp);

    auto& tcp_data = data.m_tcps[tcp];
    if (tcp_data.socket_obj) {
        tcp_data.socket_obj->bind(port);
    }
}

void connect_tcp(tcp tcp, std::string_view server_address, uint16_t server_port, tcp_callback&& callback) {
    std::unique_lock lock(g_instance->m_mutex);

    auto& data = get_loop_from_handle(tcp);

    auto& tcp_data = data.m_tcps[tcp];
    tcp_data.connect_callback = std::move(callback);
    impl::connect_tcp(data.m_context, &tcp_data, server_address, server_port);
}

void start_tcp_read(tcp tcp, tcp_read_callback&& callback) {
    std::unique_lock lock(g_instance->m_mutex);

    auto& data = get_loop_from_handle(tcp);

    auto& tcp_data = data.m_tcps[tcp];
    tcp_data.read_callback = std::move(callback);
    impl::start_tcp_read(data.m_context, &tcp_data);
}

void stop_tcp_read(tcp tcp) {
    std::unique_lock lock(g_instance->m_mutex);

    auto& data = get_loop_from_handle(tcp);

    auto& tcp_data = data.m_tcps[tcp];
    impl::stop_tcp_read(data.m_context, &tcp_data);
}

void write_tcp(tcp tcp, std::span<const uint8_t> buffer, tcp_callback&& callback) {
    std::unique_lock lock(g_instance->m_mutex);

    auto& data = get_loop_from_handle(tcp);

    auto& tcp_data = data.m_tcps[tcp];
    tcp_data.write_callback = std::move(callback);
    impl::write_tcp(data.m_context, &tcp_data, buffer);
}

tcp_server create_tcp_server(loop loop) {
    std::unique_lock lock(g_instance->m_mutex);

    auto& data = get_loop(loop);

    auto [handle, tcp_data] = data.m_tcp_servers.allocate_new();
    tcp_data->callback = tcp_server_loop_callback;
    tcp_data->socket_obj = os::create_tcp_server_socket();

    impl::add_tcp_server(data.m_context, tcp_data.get());

    data.m_tcp_servers.assign(handle, std::move(tcp_data));

    return handle;
}

void destroy_tcp_server(tcp_server tcp) {
    std::unique_lock lock(g_instance->m_mutex);

    auto& data = get_loop_from_handle(tcp);

    auto tcp_data = data.m_tcp_servers.release(tcp);
    impl::remove_tcp_server(data.m_context, tcp_data.get());

    if (tcp_data->socket_obj) {
        tcp_data->socket_obj->close();
        tcp_data->socket_obj.reset();
    }
}

void bind_tcp_server(tcp_server tcp, std::string_view addr, uint16_t port) {
    std::unique_lock lock(g_instance->m_mutex);

    auto& data = get_loop_from_handle(tcp);

    auto& tcp_data = data.m_tcp_servers[tcp];
    tcp_data.socket_obj->bind(addr, port);
}

void bind_tcp_server(tcp_server tcp, uint16_t port) {
    std::unique_lock lock(g_instance->m_mutex);

    auto& data = get_loop_from_handle(tcp);

    auto& tcp_data = data.m_tcp_servers[tcp];
    tcp_data.socket_obj->bind(port);
}

void listen_tcp(tcp_server tcp, size_t backlog, tcp_server_callback&& callback) {
    std::unique_lock lock(g_instance->m_mutex);

    auto& data = get_loop_from_handle(tcp);

    auto& tcp_data = data.m_tcp_servers[tcp];
    tcp_data.connect_callback = std::move(callback);

    tcp_data.socket_obj->listen(backlog);
}

tcp accept_tcp(tcp_server tcp) {
    std::unique_lock lock(g_instance->m_mutex);

    auto& data = get_loop_from_handle(tcp);

    auto& tcp_server_data = data.m_tcp_servers[tcp];
    auto socket = tcp_server_data.socket_obj->accept();

    auto [handle, tcp_data] = data.m_tcps.allocate_new();
    tcp_data->callback = tcp_loop_callback;
    tcp_data->socket_obj = std::move(socket);
    tcp_data->state = impl::tcp_data::state::connected;

    impl::add_tcp(data.m_context, tcp_data.get());
    data.m_tcps.assign(handle, std::move(tcp_data));

    return handle;
}

}
