
#include "looper_base.h"

namespace looper {

#define log_module looper_log_module

udp create_udp(const loop loop) {
    std::unique_lock lock(get_global_loop_data().mutex);

    auto& data = get_loop(loop);

    auto [handle, udp_impl] = data.udps.allocate_new(data.loop);
    looper_trace_info(log_module, "created new udp: loop=%lu, handle=%lu", data.handle, handle);
    data.udps.assign(handle, std::move(udp_impl));

    return handle;
}

void destroy_udp(const udp udp) {
    std::unique_lock lock(get_global_loop_data().mutex);

    auto& data = get_loop_from_handle(udp);

    looper_trace_info(log_module, "destroying udp: loop=%lu, handle=%lu", data.handle, udp);

    const auto udp_impl = data.udps.release(udp);
    udp_impl->close();
}

void bind_udp(const udp udp, const uint16_t port) {
    std::unique_lock lock(get_global_loop_data().mutex);

    auto& data = get_loop_from_handle(udp);

    looper_trace_info(log_module, "binding udp: loop=%lu, handle=%lu, port=%d", data.handle, udp, port);

    auto& udp_impl = data.udps[udp];
    udp_impl.bind(port);
}

void start_udp_read(const udp udp, udp_read_callback&& callback) {
    std::unique_lock lock(get_global_loop_data().mutex);

    auto& data = get_loop_from_handle(udp);

    looper_trace_info(log_module, "starting udp read: loop=%lu, handle=%lu", data.handle, udp);

    auto& udp_impl = data.udps[udp];
    udp_impl.start_read(std::move(callback));
}

void stop_udp_read(const udp udp) {
    std::unique_lock lock(get_global_loop_data().mutex);

    auto& data = get_loop_from_handle(udp);

    looper_trace_info(log_module, "stopping udp read: loop=%lu, handle=%lu", data.handle, udp);

    auto& udp_impl = data.udps[udp];
    udp_impl.stop_read();
}

void write_udp(const udp udp, inet_address_view destination, const std::span<const uint8_t> buffer, udp_callback&& callback) {
    std::unique_lock lock(get_global_loop_data().mutex);

    auto& data = get_loop_from_handle(udp);

    looper_trace_info(log_module, "writing to udp: loop=%lu, handle=%lu, data_size=%lu, to=%s:%d", data.handle, udp, buffer.size_bytes(), destination.ip.data(), destination.port);

    auto& udp_impl = data.udps[udp];

    const auto buffer_size = buffer.size_bytes();
    impl::udp::write_request request;
    request.destination = destination;
    request.buffer = std::unique_ptr<uint8_t[]>(new uint8_t[buffer_size]);
    request.size = buffer_size;
    request.write_callback = std::move(callback);

    memcpy(request.buffer.get(), buffer.data(), buffer_size);

    udp_impl.write(std::move(request));
}

}
