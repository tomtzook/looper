
#include "looper_base.h"

namespace looper {

#define log_module looper_log_module

tcp create_tcp(const loop loop) {
    std::unique_lock lock(get_global_loop_data().mutex);

    auto& data = get_loop(loop);

    auto [handle, tcp_impl] = data.tcps.allocate_new(data.loop);
    looper_trace_info(log_module, "created new tcp: loop=%lu, handle=%lu", data.handle, handle);
    data.tcps.assign(handle, std::move(tcp_impl));

    return handle;
}

void destroy_tcp(const tcp tcp) {
    std::unique_lock lock(get_global_loop_data().mutex);

    auto& data = get_loop_from_handle(tcp);

    looper_trace_info(log_module, "destroying tcp: loop=%lu, handle=%lu", data.handle, tcp);

    const auto tcp_impl = data.tcps.release(tcp);
    tcp_impl->close();
}

void bind_tcp(const tcp tcp, const uint16_t port) {
    std::unique_lock lock(get_global_loop_data().mutex);

    auto& data = get_loop_from_handle(tcp);

    looper_trace_info(log_module, "binding tcp: loop=%lu, handle=%lu, port=%d", data.handle, tcp, port);

    auto& tcp_impl = data.tcps[tcp];
    tcp_impl.bind(port);
}

void bind_tcp(const tcp tcp, const std::string_view address, const uint16_t port) {
    std::unique_lock lock(get_global_loop_data().mutex);

    auto& data = get_loop_from_handle(tcp);

    looper_trace_info(log_module, "binding tcp: loop=%lu, handle=%lu, address=%s:%d", data.handle, tcp, address.data(), port);

    auto& tcp_impl = data.tcps[tcp];
    tcp_impl.bind(address, port);
}

void connect_tcp(const tcp tcp, const std::string_view address, const uint16_t port, tcp_callback&& callback) {
    std::unique_lock lock(get_global_loop_data().mutex);

    auto& data = get_loop_from_handle(tcp);

    looper_trace_info(log_module, "connecting tcp: loop=%lu, handle=%lu, address=%s, port=%d", data.handle, tcp, address.data(), port);

    auto& tcp_impl = data.tcps[tcp];
    tcp_impl.connect(address, port, std::move(callback));
}

void start_tcp_read(const tcp tcp, read_callback&& callback) {
    std::unique_lock lock(get_global_loop_data().mutex);

    auto& data = get_loop_from_handle(tcp);

    looper_trace_info(log_module, "starting tcp read: loop=%lu, handle=%lu", data.handle, tcp);

    auto& tcp_impl = data.tcps[tcp];
    tcp_impl.start_read(std::move(callback));
}

void stop_tcp_read(const tcp tcp) {
    std::unique_lock lock(get_global_loop_data().mutex);

    auto& data = get_loop_from_handle(tcp);

    looper_trace_info(log_module, "stopping tcp read: loop=%lu, handle=%lu", data.handle, tcp);

    auto& tcp_impl = data.tcps[tcp];
    tcp_impl.stop_read();
}

void write_tcp(const tcp tcp, const std::span<const uint8_t> buffer, tcp_callback&& callback) {
    std::unique_lock lock(get_global_loop_data().mutex);

    auto& data = get_loop_from_handle(tcp);

    looper_trace_info(log_module, "writing to tcp: loop=%lu, handle=%lu, data_size=%lu", data.handle, tcp, buffer.size_bytes());

    auto& tcp_impl = data.tcps[tcp];

    const auto buffer_size = buffer.size_bytes();
    impl::stream::write_request request;
    request.buffer = std::unique_ptr<uint8_t[]>(new uint8_t[buffer_size]);
    request.pos = 0;
    request.size = buffer.size_bytes();
    request.write_callback = std::move(callback);

    memcpy(request.buffer.get(), buffer.data(), buffer_size);

    tcp_impl.write(std::move(request));
}

tcp_server create_tcp_server(const loop loop) {
    std::unique_lock lock(get_global_loop_data().mutex);

    auto& data = get_loop(loop);

    auto [handle, tcp_impl] = data.tcp_servers.allocate_new(data.loop);
    looper_trace_info(log_module, "creating new tcp server: loop=%lu, handle=%lu", data.handle, handle);
    data.tcp_servers.assign(handle, std::move(tcp_impl));

    return handle;
}

void destroy_tcp_server(const tcp_server tcp) {
    std::unique_lock lock(get_global_loop_data().mutex);

    auto& data = get_loop_from_handle(tcp);

    looper_trace_info(log_module, "destroying tcp server: loop=%lu, handle=%lu", data.handle, tcp);

    const auto tcp_impl = data.tcp_servers.release(tcp);
    tcp_impl->close();
}

void bind_tcp_server(const tcp_server tcp, const std::string_view address, const uint16_t port) {
    std::unique_lock lock(get_global_loop_data().mutex);

    auto& data = get_loop_from_handle(tcp);

    looper_trace_info(log_module, "binding tcp server: loop=%lu, handle=%lu, address=%s, port=%d", data.handle, tcp, address.data(), port);

    auto& tcp_impl = data.tcp_servers[tcp];
    tcp_impl.bind(address, port);
}

void bind_tcp_server(const tcp_server tcp, const uint16_t port) {
    std::unique_lock lock(get_global_loop_data().mutex);

    auto& data = get_loop_from_handle(tcp);

    looper_trace_info(log_module, "binding tcp server: loop=%lu, handle=%lu, port=%d", data.handle, tcp, port);

    auto& tcp_impl = data.tcp_servers[tcp];
    tcp_impl.bind(port);
}

void listen_tcp(const tcp_server tcp, const size_t backlog, tcp_server_callback&& callback) {
    std::unique_lock lock(get_global_loop_data().mutex);

    auto& data = get_loop_from_handle(tcp);

    looper_trace_info(log_module, "start listen on tcp server: loop=%lu, handle=%lu, backlog=%lu", data.handle, tcp, backlog);

    auto& tcp_impl = data.tcp_servers[tcp];
    tcp_impl.listen(backlog, std::move(callback));
}

tcp accept_tcp(const tcp_server tcp) {
    std::unique_lock lock(get_global_loop_data().mutex);

    auto& data = get_loop_from_handle(tcp);

    looper_trace_info(log_module, "accepting on tcp server: loop=%lu, handle=%lu", data.handle, tcp);

    auto& server_impl = data.tcp_servers[tcp];
    const auto client_handle = data.tcps.reserve();
    auto client = server_impl.accept(client_handle);
    data.tcps.assign(client_handle, std::move(client));

    looper_trace_info(log_module, "new tcp accepted: loop=%lu, server=%lu, client=%lu", data.handle, tcp, client_handle);

    return client_handle;
}

}
