#include <optional>
#include <thread>

#include <looper.h>
#include <looper_tcp.h>
#include <cstring>

#include "util/handles.h"
#include "util/util.h"
#include "os/factory.h"
#include "loop.h"

namespace looper {

#define log_module "looper"

static constexpr size_t handle_counts_per_type = 64;
static constexpr size_t loops_count = 8;

struct loop_data {
    explicit loop_data(loop handle)
        : m_handle(handle)
        , m_context(impl::create_loop())
        , m_closing(false)
        , m_thread(nullptr)
        , m_events(handles::handle{handle}.index(), handles::type_event)
        , m_timers(handles::handle{handle}.index(), handles::type_timer)
        , m_futures(handles::handle{handle}.index(), handles::type_future)
        , m_tcps(handles::handle{handle}.index(), handles::type_tcp)
        , m_tcp_servers(handles::handle{handle}.index(), handles::type_tcp_server)
    {}
    ~loop_data() {
        if (m_thread && m_thread->joinable()) {
            m_thread->join();
        }
        m_thread.reset();

        clear_context();
    }

    loop_data(const loop_data&) = delete;
    loop_data(loop_data&&) = delete;
    loop_data& operator=(const loop_data&) = delete;
    loop_data& operator=(loop_data&&) = delete;

    void clear_context() {
        if (m_context != nullptr) {
            impl::destroy_loop(m_context);
            m_context = nullptr;
        }
    }

    loop m_handle;
    impl::loop_context* m_context;
    bool m_closing;

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

    looper_data(const looper_data&) = delete;
    looper_data(looper_data&&) = delete;
    looper_data& operator=(const looper_data&) = delete;
    looper_data& operator=(looper_data&&) = delete;

    // todo: we use this mutex everywhere, could be problematic, limit use. perhaps remove lock from loop layer, how?
    //  could use some lock-less mechanisms, or spinlocks
    std::mutex m_mutex;
    handles::handle_table<loop_data, loops_count> m_loops;
};

static inline looper_data& get_global_loop_data() {
    static looper_data g_instance{};
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
        std::unique_lock lock(get_global_loop_data().m_mutex);
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

    looper_trace_debug(log_module, "future callback called: loop=%lu, handle=%lu", loop, future->handle);
    invoke_func_nolock("future_user_callback", future->user_callback, loop, future->handle);

    future->exec_finished.notify_all();
}

static void event_loop_callback(impl::event_data* event) {
    auto loop = get_loop_handle(event->handle);

    looper_trace_debug(log_module, "event callback called: loop=%lu, handle=%lu", loop, event->handle);
    invoke_func_nolock("event_user_callback", event->user_callback, loop, event->handle);
}

static void timer_loop_callback(impl::timer_data* timer) {
    auto loop = get_loop_handle(timer->handle);

    looper_trace_debug(log_module, "timer callback called: loop=%lu, handle=%lu", loop, timer->handle);
    invoke_func_nolock("timer_user_callback", timer->user_callback, loop, timer->handle);
}

static void _tcp_read_callback(impl::tcp_data* tcp, std::span<const uint8_t> buffer, looper::error error) {
    auto loop = get_loop_handle(tcp->handle);

    looper_trace_debug(log_module, "tcp read new data: loop=%lu, handle=%lu, error=%lu", loop, tcp->handle, error);
    invoke_func_nolock("tcp_read_callback", tcp->read_callback, loop, tcp->handle, buffer, error);
}

static void _tcp_write_callback(impl::tcp_data* tcp, impl::tcp_data::write_request& request, looper::error error) {
    auto loop = get_loop_handle(tcp->handle);

    looper_trace_debug(log_module, "tcp writing finished: loop=%lu, handle=%lu, error=%lu", loop, tcp->handle, error);
    invoke_func_nolock("tcp_write_callback", request.write_callback, loop, tcp->handle, error);
}

static void _tcp_connect_callback(impl::tcp_data* tcp, looper::error error) {
    auto loop = get_loop_handle(tcp->handle);

    looper_trace_debug(log_module, "tcp connect finished: loop=%lu, handle=%lu, error=%lu", loop, tcp->handle, error);
    invoke_func_nolock("tcp_connect_callback", tcp->connect_callback, loop, tcp->handle, error);
}

static void _tcp_error_callback(impl::tcp_data* tcp, looper::error error) {
    auto loop = get_loop_handle(tcp->handle);

    looper_trace_debug(log_module, "tcp hung/error: loop=%lu, handle=%lu, error=%lu", loop, tcp->handle, error);
    // todo: what now? close socket and report?
}

static void tcp_server_loop_callback(impl::tcp_server_data* tcp) {
    auto loop = get_loop_handle(tcp->handle);

    looper_trace_debug(log_module, "tcp server callback called: loop=%lu, handle=%lu", loop, tcp->handle);
    invoke_func_nolock("tcp_accept_user_callback", tcp->connect_callback, loop, tcp->handle);
}

static future create_future_internal(loop loop, future_callback&& callback) {
    auto& data = get_loop(loop);

    auto [handle, future_data] = data.m_futures.allocate_new();
    future_data->user_callback = std::move(callback);
    future_data->from_loop_callback = future_loop_callback;

    looper_trace_info(log_module, "creating future: loop=%lu, handle=%lu", loop, handle);

    impl::add_future(data.m_context, future_data.get());

    data.m_futures.assign(handle, std::move(future_data));

    return handle;
}

static void destroy_future_internal(future future) {
    auto& data = get_loop_from_handle(future);

    looper_trace_info(log_module, "destroying future: loop=%lu, handle=%lu", data.m_handle, future);

    auto future_data = data.m_futures.release(future);
    impl::remove_future(data.m_context, future_data.get());
}

static void execute_future_internal(future future, std::chrono::milliseconds delay) {
    auto& data = get_loop_from_handle(future);

    auto& future_data = data.m_futures[future];
    if (!future_data.finished) {
        throw std::runtime_error("future already queued for execution");
    }

    looper_trace_info(log_module, "requesting future execution: loop=%lu, handle=%lu, delay=%lu", data.m_handle, future, delay.count());

    future_data.delay = delay;
    impl::exec_future(data.m_context, &future_data);
}

static bool wait_for_future_internal(std::unique_lock<std::mutex>& lock, future future, std::chrono::milliseconds timeout) {
    auto& data = get_loop_from_handle(future);

    auto& future_data = data.m_futures[future];
    if (future_data.finished) {
        looper_trace_debug(log_module, "future already finished, not waiting: loop=%lu, handle=%lu", data.m_handle, future);
        return false;
    }

    looper_trace_info(log_module, "waiting on future: loop=%lu, handle=%lu, timeout=%lu", data.m_handle, future, timeout.count());

    return !future_data.exec_finished.wait_for(lock, timeout, [future]()->bool {
        auto& data = get_loop_from_handle(future);

        if (!data.m_futures.has(future)) {
            return true;
        }

        auto& future_data = data.m_futures[future];
        return future_data.finished;
    });
}

loop create() {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto [handle, data] = get_global_loop_data().m_loops.allocate_new();
    get_global_loop_data().m_loops.assign(handle, std::move(data));

    looper_trace_info(log_module, "created new loop: handle=%lu", handle);

    return handle;
}

void destroy(loop loop) {
    std::unique_lock lock(get_global_loop_data().m_mutex);
    auto& data = get_loop(loop);
    data.m_closing = true;

    looper_trace_info(log_module, "destroying loop: handle=%lu", loop);

    auto thread = std::move(data.m_thread);
    lock.unlock();

    if (thread && thread->joinable()) {
        looper_trace_debug(log_module, "loop running in thread, joining: handle=%lu", loop);
        thread->join();
    }

    data.clear_context();
    lock.lock();

    get_global_loop_data().m_loops.release(loop);

    looper_trace_info(log_module, "loop destroyed: handle=%lu", loop);
}

void run_once(loop loop) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop(loop);
    if (data.m_thread) {
        throw std::runtime_error("loop running in thread");
    }

    looper_trace_debug(log_module, "running loop once: handle=%lu", loop);

    lock.unlock();
    impl::run_once(data.m_context);
}

void run_forever(loop loop) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop(loop);
    if (data.m_thread) {
        throw std::runtime_error("loop running in thread");
    }

    looper_trace_info(log_module, "running loop forever: handle=%lu", loop);

    lock.unlock();
    run_loop_forever(loop);
}

void exec_in_thread(loop loop) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop(loop);
    if (data.m_thread) {
        looper_trace_debug(log_module, "loop already running in thread: handle=%lu", loop);
        return;
    }

    looper_trace_info(log_module, "starting loop execution in thread: handle=%lu", loop);

    data.m_thread = std::make_unique<std::thread>(&thread_main, loop);
}

// execute
future create_future(loop loop, future_callback&& callback) {
    std::unique_lock lock(get_global_loop_data().m_mutex);
    return create_future_internal(loop, std::move(callback));
}

void destroy_future(future future) {
    std::unique_lock lock(get_global_loop_data().m_mutex);
    destroy_future_internal(future);
}

void execute_once(future future, std::chrono::milliseconds delay) {
    std::unique_lock lock(get_global_loop_data().m_mutex);
    execute_future_internal(future, delay);
}

bool wait_for(future future, std::chrono::milliseconds timeout) {
    std::unique_lock lock(get_global_loop_data().m_mutex);
    return wait_for_future_internal(lock, future, timeout);
}

void execute_later(loop loop, loop_callback&& callback) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto future = create_future_internal(loop, [callback](looper::loop loop, looper::future future)->void {
        std::unique_lock lock(get_global_loop_data().m_mutex);
        destroy_future_internal(future);

        invoke_func(lock, "future_singleuse_callback", callback, loop);
    });
    execute_future_internal(future, no_delay);
}

bool execute_later_and_wait(loop loop, loop_callback&& callback, std::chrono::milliseconds timeout) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto future = create_future_internal(loop, [callback](looper::loop loop, looper::future future)->void {
        std::unique_lock lock(get_global_loop_data().m_mutex);
        destroy_future_internal(future);

        invoke_func(lock, "future_singleuse_callback", callback, loop);
    });
    execute_future_internal(future, no_delay);

    return wait_for_future_internal(lock, future, timeout);
}

// events
event create_event(loop loop, event_callback&& callback) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop(loop);

    auto [handle, event_data] = data.m_events.allocate_new();
    event_data->user_callback = std::move(callback);
    event_data->event_obj = os::create_event();
    event_data->from_loop_callback = event_loop_callback;

    looper_trace_info(log_module, "creating new event: loop=%lu, handle=%lu", loop, handle);

    impl::add_event(data.m_context, event_data.get());

    data.m_events.assign(handle, std::move(event_data));

    return handle;
}

void destroy_event(event event) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop_from_handle(event);

    looper_trace_info(log_module, "destroying event: loop=%lu, handle=%lu", data.m_handle, event);

    auto event_data = data.m_events.release(event);
    impl::remove_event(data.m_context, event_data.get());
}

void set_event(event event) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop_from_handle(event);

    looper_trace_debug(log_module, "setting event: loop=%lu, handle=%lu", data.m_handle, event);

    auto& event_data = data.m_events[event];
    event_data.event_obj->set();
}

void clear_event(event event) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop_from_handle(event);

    looper_trace_debug(log_module, "clearing event: loop=%lu, handle=%lu", data.m_handle, event);

    auto& event_data = data.m_events[event];
    event_data.event_obj->clear();
}

// timers
timer create_timer(loop loop, std::chrono::milliseconds timeout, timer_callback&& callback) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop(loop);

    auto [handle, timer_data] = data.m_timers.assign_new();
    timer_data.user_callback = std::move(callback);
    timer_data.timeout = timeout;
    timer_data.from_loop_callback = timer_loop_callback;

    looper_trace_info(log_module, "creating new timer: loop=%lu, handle=%lu, timeout=%lu", data.m_handle, handle, timeout.count());

    return handle;
}

void destroy_timer(timer timer) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop_from_handle(timer);

    looper_trace_info(log_module, "destroying timer: loop=%lu, handle=%lu", data.m_handle, timer);

    auto timer_data = data.m_timers.release(timer);
    if (timer_data->running) {
        impl::remove_timer(data.m_context, timer_data.get());
    }
}

void start_timer(timer timer) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop_from_handle(timer);

    auto& timer_data = data.m_timers[timer];
    if (!timer_data.running) {
        looper_trace_debug(log_module, "starting timer: loop=%lu, handle=%lu", data.m_handle, timer);

        impl::add_timer(data.m_context, &timer_data);
    }
}

void stop_timer(timer timer) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop_from_handle(timer);

    auto& timer_data = data.m_timers[timer];
    if (timer_data.running) {
        looper_trace_debug(log_module, "stopping timer: loop=%lu, handle=%lu", data.m_handle, timer);

        impl::remove_timer(data.m_context, &timer_data);
    }
}

void reset_timer(timer timer) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop_from_handle(timer);

    auto& timer_data = data.m_timers[timer];
    if (timer_data.running) {
        looper_trace_debug(log_module, "resetting timer: loop=%lu, handle=%lu", data.m_handle, timer);

        impl::reset_timer(data.m_context, &timer_data);
    }
}

// tcp
tcp create_tcp(loop loop) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop(loop);

    auto [handle, tcp_data] = data.m_tcps.allocate_new();
    tcp_data->l_read_callback = _tcp_read_callback;
    tcp_data->l_write_callback = _tcp_write_callback;
    tcp_data->l_connect_callback = _tcp_connect_callback;
    tcp_data->l_error_callback = _tcp_error_callback;
    tcp_data->socket_obj = os::create_tcp_socket();
    tcp_data->state = impl::tcp_data::state::open;

    looper_trace_info(log_module, "creating new tcp: loop=%lu, handle=%lu", data.m_handle, handle);

    impl::add_tcp(data.m_context, tcp_data.get());

    data.m_tcps.assign(handle, std::move(tcp_data));

    return handle;
}

void destroy_tcp(tcp tcp) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop_from_handle(tcp);

    looper_trace_info(log_module, "destroying tcp: loop=%lu, handle=%lu", data.m_handle, tcp);

    auto tcp_data = data.m_tcps.release(tcp);
    impl::remove_tcp(data.m_context, tcp_data.get());

    if (tcp_data->socket_obj) {
        tcp_data->socket_obj->close();
        tcp_data->socket_obj.reset();
    }
}

void bind_tcp(tcp tcp, uint16_t port) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop_from_handle(tcp);

    auto& tcp_data = data.m_tcps[tcp];
    if (tcp_data.socket_obj) {
        looper_trace_info(log_module, "binding tcp: loop=%lu, handle=%lu, port=%d", data.m_handle, tcp, port);

        tcp_data.socket_obj->bind(port);
    }
}

void connect_tcp(tcp tcp, std::string_view server_address, uint16_t server_port, tcp_callback&& callback) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop_from_handle(tcp);

    looper_trace_info(log_module, "connecting tcp: loop=%lu, handle=%lu, address=%s, port=%d", data.m_handle, tcp, server_address.data(), server_port);

    auto& tcp_data = data.m_tcps[tcp];
    tcp_data.connect_callback = std::move(callback);
    impl::connect_tcp(data.m_context, &tcp_data, server_address, server_port);
}

void start_tcp_read(tcp tcp, tcp_read_callback&& callback) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop_from_handle(tcp);

    looper_trace_info(log_module, "starting tcp read: loop=%lu, handle=%lu", data.m_handle, tcp);

    auto& tcp_data = data.m_tcps[tcp];
    tcp_data.read_callback = std::move(callback);
    impl::start_tcp_read(data.m_context, &tcp_data);
}

void stop_tcp_read(tcp tcp) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop_from_handle(tcp);

    looper_trace_info(log_module, "stopping tcp read: loop=%lu, handle=%lu", data.m_handle, tcp);

    auto& tcp_data = data.m_tcps[tcp];
    impl::stop_tcp_read(data.m_context, &tcp_data);
}

void write_tcp(tcp tcp, std::span<const uint8_t> buffer, tcp_callback&& callback) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop_from_handle(tcp);

    looper_trace_info(log_module, "writing to tcp: loop=%lu, handle=%lu, data_size=%lu", data.m_handle, tcp, buffer.size_bytes());

    auto& tcp_data = data.m_tcps[tcp];

    const auto buffer_size = buffer.size_bytes();
    impl::tcp_data::write_request request;
    request.buffer = std::unique_ptr<uint8_t[]>(new uint8_t[buffer_size]);
    request.pos = 0;
    request.size = buffer.size_bytes();
    request.write_callback = std::move(callback);

    memcpy(request.buffer.get(), buffer.data(), buffer_size);

    impl::write_tcp(data.m_context, &tcp_data, std::move(request));
}

tcp_server create_tcp_server(loop loop) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop(loop);

    auto [handle, tcp_data] = data.m_tcp_servers.allocate_new();
    tcp_data->callback = tcp_server_loop_callback;
    tcp_data->socket_obj = os::create_tcp_server_socket();

    looper_trace_info(log_module, "creating new tcp server: loop=%lu, handle=%lu", data.m_handle, handle);

    impl::add_tcp_server(data.m_context, tcp_data.get());

    data.m_tcp_servers.assign(handle, std::move(tcp_data));

    return handle;
}

void destroy_tcp_server(tcp_server tcp) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop_from_handle(tcp);

    looper_trace_info(log_module, "destroying tcp server: loop=%lu, handle=%lu", data.m_handle, tcp);

    auto tcp_data = data.m_tcp_servers.release(tcp);
    impl::remove_tcp_server(data.m_context, tcp_data.get());

    if (tcp_data->socket_obj) {
        tcp_data->socket_obj->close();
        tcp_data->socket_obj.reset();
    }
}

void bind_tcp_server(tcp_server tcp, std::string_view addr, uint16_t port) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop_from_handle(tcp);

    looper_trace_info(log_module, "binding tcp server: loop=%lu, handle=%lu, address=%s, port=%d", data.m_handle, tcp, addr.data(), port);

    auto& tcp_data = data.m_tcp_servers[tcp];
    tcp_data.socket_obj->bind(addr, port);
}

void bind_tcp_server(tcp_server tcp, uint16_t port) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop_from_handle(tcp);

    looper_trace_info(log_module, "binding tcp server: loop=%lu, handle=%lu, port=%d", data.m_handle, tcp, port);

    auto& tcp_data = data.m_tcp_servers[tcp];
    tcp_data.socket_obj->bind(port);
}

void listen_tcp(tcp_server tcp, size_t backlog, tcp_server_callback&& callback) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop_from_handle(tcp);

    looper_trace_info(log_module, "start listen on tcp server: loop=%lu, handle=%lu, backlog=%lu", data.m_handle, tcp, backlog);

    auto& tcp_data = data.m_tcp_servers[tcp];
    tcp_data.connect_callback = std::move(callback);

    tcp_data.socket_obj->listen(backlog);
}

tcp accept_tcp(tcp_server tcp) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop_from_handle(tcp);

    looper_trace_info(log_module, "accepting on tcp server: loop=%lu, handle=%lu", data.m_handle, tcp);

    auto& tcp_server_data = data.m_tcp_servers[tcp];
    auto socket = tcp_server_data.socket_obj->accept();

    auto [handle, tcp_data] = data.m_tcps.allocate_new();
    tcp_data->l_read_callback = _tcp_read_callback;
    tcp_data->l_write_callback = _tcp_write_callback;
    tcp_data->l_connect_callback = _tcp_connect_callback;
    tcp_data->l_error_callback = _tcp_error_callback;
    tcp_data->socket_obj = std::move(socket);
    tcp_data->state = impl::tcp_data::state::connected;

    looper_trace_info(log_module, "new tcp accepted: loop=%lu, server=%lu, client=%lu", data.m_handle, tcp, handle);

    impl::add_tcp(data.m_context, tcp_data.get());
    data.m_tcps.assign(handle, std::move(tcp_data));

    return handle;
}

}
