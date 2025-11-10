
#include "looper_base.h"

namespace looper {

#define log_module looper_log_module

unix_socket create_unix_socket(const loop loop) {
    std::unique_lock lock(get_global_loop_data().mutex);

    auto& data = get_loop(loop);

    auto [handle, unix_socket_impl] = data.unix_sockets.allocate_new(data.loop);
    looper_trace_info(log_module, "created new unix_socket: loop=%lu, handle=%lu", data.handle, handle);
    data.unix_sockets.assign(handle, std::move(unix_socket_impl));

    return handle;
}

void destroy_unix_socket(const unix_socket unix_socket) {
    std::unique_lock lock(get_global_loop_data().mutex);

    auto& data = get_loop_from_handle(unix_socket);

    looper_trace_info(log_module, "destroying unix_socket: loop=%lu, handle=%lu", data.handle, unix_socket);

    const auto unix_socket_impl = data.unix_sockets.release(unix_socket);
    unix_socket_impl->close();
}

void bind_unix_socket(const unix_socket unix_socket, const std::string_view path) {
    std::unique_lock lock(get_global_loop_data().mutex);

    auto& data = get_loop_from_handle(unix_socket);

    looper_trace_info(log_module, "binding unix_socket: loop=%lu, handle=%lu, path=%s", data.handle, unix_socket, path.data());

    auto& unix_socket_impl = data.unix_sockets[unix_socket];
    unix_socket_impl.bind(path);
}

void connect_unix_socket(const unix_socket unix_socket, const std::string_view path, connect_callback&& callback) {
    std::unique_lock lock(get_global_loop_data().mutex);

    auto& data = get_loop_from_handle(unix_socket);

    looper_trace_info(log_module, "connecting unix_socket: loop=%lu, handle=%lu, path=%s", data.handle, unix_socket, path.data());

    auto& unix_socket_impl = data.unix_sockets[unix_socket];
    unix_socket_impl.connect(std::move(callback), path);
}

void start_unix_socket_read(const unix_socket unix_socket, read_callback&& callback) {
    std::unique_lock lock(get_global_loop_data().mutex);

    auto& data = get_loop_from_handle(unix_socket);

    looper_trace_info(log_module, "starting unix_socket read: loop=%lu, handle=%lu", data.handle, unix_socket);

    auto& unix_socket_impl = data.unix_sockets[unix_socket];
    unix_socket_impl.start_read(std::move(callback));
}

void stop_unix_socket_read(const unix_socket unix_socket) {
    std::unique_lock lock(get_global_loop_data().mutex);

    auto& data = get_loop_from_handle(unix_socket);

    looper_trace_info(log_module, "stopping unix_socket read: loop=%lu, handle=%lu", data.handle, unix_socket);

    auto& unix_socket_impl = data.unix_sockets[unix_socket];
    unix_socket_impl.stop_read();
}

void write_unix_socket(const unix_socket unix_socket, const std::span<const uint8_t> buffer, unix_socket_callback&& callback) {
    std::unique_lock lock(get_global_loop_data().mutex);

    auto& data = get_loop_from_handle(unix_socket);

    looper_trace_info(log_module, "writing to unix_socket: loop=%lu, handle=%lu, data_size=%lu", data.handle, unix_socket, buffer.size_bytes());

    auto& unix_socket_impl = data.unix_sockets[unix_socket];

    const auto buffer_size = buffer.size_bytes();
    impl::stream_write_request request{};
    request.buffer = std::unique_ptr<uint8_t[]>(new uint8_t[buffer_size]);
    request.pos = 0;
    request.size = buffer.size_bytes();
    request.write_callback = std::move(callback);

    memcpy(request.buffer.get(), buffer.data(), buffer_size);

    unix_socket_impl.write(std::move(request));
}

unix_socket_server create_unix_socket_server(const loop loop) {
    std::unique_lock lock(get_global_loop_data().mutex);

    auto& data = get_loop(loop);

    auto [handle, unix_socket_impl] = data.unix_socket_servers.allocate_new(data.loop);
    looper_trace_info(log_module, "creating new unix_socket server: loop=%lu, handle=%lu", data.handle, handle);
    data.unix_socket_servers.assign(handle, std::move(unix_socket_impl));

    return handle;
}

void destroy_unix_socket_server(const unix_socket_server unix_socket) {
    std::unique_lock lock(get_global_loop_data().mutex);

    auto& data = get_loop_from_handle(unix_socket);

    looper_trace_info(log_module, "destroying unix_socket server: loop=%lu, handle=%lu", data.handle, unix_socket);

    const auto unix_socket_impl = data.unix_socket_servers.release(unix_socket);
    unix_socket_impl->close();
}

void bind_unix_socket_server(const unix_socket_server unix_socket, const std::string_view path) {
    std::unique_lock lock(get_global_loop_data().mutex);

    auto& data = get_loop_from_handle(unix_socket);

    looper_trace_info(log_module, "binding unix_socket server: loop=%lu, handle=%lu, path=%s", data.handle, unix_socket, path.data());

    auto& unix_socket_impl = data.unix_socket_servers[unix_socket];
    unix_socket_impl.bind(path);
}

void listen_unix_socket(const unix_socket_server unix_socket, const size_t backlog, listen_callback&& callback) {
    std::unique_lock lock(get_global_loop_data().mutex);

    auto& data = get_loop_from_handle(unix_socket);

    looper_trace_info(log_module, "start listen on unix_socket server: loop=%lu, handle=%lu, backlog=%lu", data.handle, unix_socket, backlog);

    auto& unix_socket_impl = data.unix_socket_servers[unix_socket];
    unix_socket_impl.listen(backlog, std::move(callback));
}

unix_socket accept_unix_socket(const unix_socket_server unix_socket) {
    std::unique_lock lock(get_global_loop_data().mutex);

    auto& data = get_loop_from_handle(unix_socket);

    looper_trace_info(log_module, "accepting on unix_socket server: loop=%lu, handle=%lu", data.handle, unix_socket);

    auto& server_impl = data.unix_socket_servers[unix_socket];
    const auto client_handle = data.unix_sockets.reserve();
    auto client = server_impl.accept(client_handle);
    data.unix_sockets.assign(client_handle, std::move(client));

    looper_trace_info(log_module, "new unix_socket accepted: loop=%lu, server=%lu, client=%lu", data.handle, unix_socket, client_handle);

    return client_handle;
}

}
