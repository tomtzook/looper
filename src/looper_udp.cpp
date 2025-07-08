
#include "looper_base.h"

namespace looper {

#define log_module looper_log_module

static void _file_read_callback(impl::udp_data* udp, const inet_address& sender, std::span<const uint8_t> buffer, looper::error error) {
    auto loop = get_loop_handle(udp->handle);

    looper_trace_debug(log_module, "udp read new data: loop=%lu, handle=%lu, error=%lu", loop, udp->handle, error);
    invoke_func_nolock("udp_read_callback", udp->user_read_callback, loop, udp->handle, sender, buffer, error);
}

static void _file_write_callback(impl::udp_data* udp, impl::udp_data::write_request& request) {
    auto loop = get_loop_handle(udp->handle);

    looper_trace_debug(log_module, "udp writing finished: loop=%lu, handle=%lu, error=%lu", loop, udp->handle, request.error);
    invoke_func_nolock("udp_write_callback", request.write_callback, loop, udp->handle, request.error);
}

udp create_udp(loop loop) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop(loop);

    auto [handle, udp_data] = data.m_udps.allocate_new();
    udp_data->socket_obj = os::make_udp();
    udp_data->from_loop_read_callback = _file_read_callback;
    udp_data->from_loop_write_callback = _file_write_callback;
    udp_data->state = impl::udp_data::state::open;

    looper_trace_info(log_module, "creating new udp: loop=%lu, handle=%lu", data.m_handle, handle);

    impl::add_udp(data.m_context, udp_data.get());

    data.m_udps.assign(handle, std::move(udp_data));

    return handle;
}

void destroy_udp(udp udp) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop_from_handle(udp);

    looper_trace_info(log_module, "destroying udp: loop=%lu, handle=%lu", data.m_handle, udp);

    auto udp_data = data.m_udps.release(udp);
    impl::remove_udp(data.m_context, udp_data.get());

    if (udp_data->socket_obj) {
        udp_data->socket_obj.reset();
    }
}

void bind_udp(udp udp, uint16_t port) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop_from_handle(udp);

    auto& udp_data = data.m_udps[udp];
    if (udp_data.socket_obj) {
        looper_trace_info(log_module, "binding udp: loop=%lu, handle=%lu, port=%d", data.m_handle, udp, port);
        OS_CHECK_THROW(os::udp::bind(udp_data.socket_obj.get(), port));
    }
}

void start_udp_read(udp udp, udp_read_callback&& callback) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop_from_handle(udp);

    looper_trace_info(log_module, "starting udp read: loop=%lu, handle=%lu", data.m_handle, udp);

    auto& udp_data = data.m_udps[udp];
    udp_data.user_read_callback = std::move(callback);
    impl::start_udp_read(data.m_context, &udp_data);
}

void stop_udp_read(udp udp) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop_from_handle(udp);

    looper_trace_info(log_module, "stopping udp read: loop=%lu, handle=%lu", data.m_handle, udp);

    auto& udp_data = data.m_udps[udp];
    impl::stop_udp_read(data.m_context, &udp_data);
}

void write_udp(udp udp, inet_address destination, std::span<const uint8_t> buffer, udp_callback&& callback) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop_from_handle(udp);

    looper_trace_info(log_module, "writing to udp: loop=%lu, handle=%lu, data_size=%lu", data.m_handle, udp, buffer.size_bytes());

    auto& udp_data = data.m_udps[udp];

    const auto buffer_size = buffer.size_bytes();
    impl::udp_data::write_request request;
    request.destination = destination;
    request.buffer = std::unique_ptr<uint8_t[]>(new uint8_t[buffer_size]);
    request.size = buffer_size;
    request.write_callback = std::move(callback);

    memcpy(request.buffer.get(), buffer.data(), buffer_size);

    impl::write_udp(data.m_context, &udp_data, std::move(request));
}

}
