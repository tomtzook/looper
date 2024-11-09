
#include "looper_base.h"

namespace looper {

#define log_module looper_log_module

static void _tcp_read_callback(impl::tcp_data* tcp, std::span<const uint8_t> buffer, looper::error error) {
    auto loop = get_loop_handle(tcp->handle);

    looper_trace_debug(log_module, "tcp read new data: loop=%lu, handle=%lu, error=%lu", loop, tcp->handle, error);
    invoke_func_nolock("tcp_read_callback", tcp->read_callback, loop, tcp->handle, buffer, error);
}

static void _tcp_write_callback(impl::tcp_data* tcp, impl::tcp_data::write_request& request) {
    auto loop = get_loop_handle(tcp->handle);

    looper_trace_debug(log_module, "tcp writing finished: loop=%lu, handle=%lu, error=%lu", loop, tcp->handle, request.error);
    invoke_func_nolock("tcp_write_callback", request.write_callback, loop, tcp->handle, request.error);
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

tcp create_tcp(loop loop) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop(loop);

    auto [handle, tcp_data] = data.m_tcps.allocate_new();
    tcp_data->l_read_callback = _tcp_read_callback;
    tcp_data->l_write_callback = _tcp_write_callback;
    tcp_data->l_connect_callback = _tcp_connect_callback;
    tcp_data->l_error_callback = _tcp_error_callback;
    tcp_data->socket_obj = os::make_tcp();
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
        tcp_data->socket_obj.reset();
    }
}

void bind_tcp(tcp tcp, uint16_t port) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop_from_handle(tcp);

    auto& tcp_data = data.m_tcps[tcp];
    if (tcp_data.socket_obj) {
        looper_trace_info(log_module, "binding tcp: loop=%lu, handle=%lu, port=%d", data.m_handle, tcp, port);
        OS_CHECK_THROW(os::tcp::bind(tcp_data.socket_obj.get(), port));
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
    tcp_data->socket_obj = os::make_tcp();

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
        tcp_data->socket_obj.reset();
    }
}

void bind_tcp_server(tcp_server tcp, std::string_view addr, uint16_t port) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop_from_handle(tcp);

    looper_trace_info(log_module, "binding tcp server: loop=%lu, handle=%lu, address=%s, port=%d", data.m_handle, tcp, addr.data(), port);

    auto& tcp_data = data.m_tcp_servers[tcp];
    OS_CHECK_THROW(os::tcp::bind(tcp_data.socket_obj.get(), addr, port));
}

void bind_tcp_server(tcp_server tcp, uint16_t port) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop_from_handle(tcp);

    looper_trace_info(log_module, "binding tcp server: loop=%lu, handle=%lu, port=%d", data.m_handle, tcp, port);

    auto& tcp_data = data.m_tcp_servers[tcp];
    OS_CHECK_THROW(os::tcp::bind(tcp_data.socket_obj.get(), port));
}

void listen_tcp(tcp_server tcp, size_t backlog, tcp_server_callback&& callback) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop_from_handle(tcp);

    looper_trace_info(log_module, "start listen on tcp server: loop=%lu, handle=%lu, backlog=%lu", data.m_handle, tcp, backlog);

    auto& tcp_data = data.m_tcp_servers[tcp];
    tcp_data.connect_callback = std::move(callback);
    OS_CHECK_THROW(os::tcp::listen(tcp_data.socket_obj.get(), backlog));
}

tcp accept_tcp(tcp_server tcp) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop_from_handle(tcp);

    looper_trace_info(log_module, "accepting on tcp server: loop=%lu, handle=%lu", data.m_handle, tcp);

    auto& tcp_server_data = data.m_tcp_servers[tcp];
    os::tcp::tcp* tcp_struct;
    OS_CHECK_THROW(os::tcp::accept(tcp_server_data.socket_obj.get(), &tcp_struct));
    auto socket = os::make_tcp(tcp_struct);

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
