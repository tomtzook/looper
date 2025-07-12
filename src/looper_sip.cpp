
#include <looper_sip.h>
#include "looper_base.h"

namespace looper::sip {

#define log_module looper_log_module

sip_session create_sip(const loop loop, transport transport) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop(loop);

    auto [handle, sip_impl] = data.m_sip_sessions.allocate_new(data.m_context, transport);
    looper_trace_info(log_module, "created new sip session: loop=%lu, handle=%lu", data.m_handle, handle);
    data.m_sip_sessions.assign(handle, std::move(sip_impl));

    return handle;
}

sip_session create_sip_tcp(const loop loop, const tcp tcp) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop(loop);
    auto tcp_impl = data.m_tcps.share(tcp);

    auto [handle, sip_impl] = data.m_sip_sessions.allocate_new(data.m_context, tcp_impl);
    looper_trace_info(log_module, "created new sip session: loop=%lu, handle=%lu", data.m_handle, handle);
    data.m_sip_sessions.assign(handle, std::move(sip_impl));

    return handle;
}

sip_session create_sip_udp(const loop loop, const udp udp) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop(loop);
    auto udp_impl = data.m_udps.share(udp);

    auto [handle, sip_impl] = data.m_sip_sessions.allocate_new(data.m_context, udp_impl);
    looper_trace_info(log_module, "created new sip session: loop=%lu, handle=%lu", data.m_handle, handle);
    data.m_sip_sessions.assign(handle, std::move(sip_impl));

    return handle;
}

void destroy_sip(const sip_session sip) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop_from_handle(sip);

    looper_trace_info(log_module, "destroying sip session: loop=%lu, handle=%lu", data.m_handle, sip);

    const auto sip_impl = data.m_sip_sessions.release(sip);
    sip_impl->close();
}

void open(const sip_session sip, inet_address_view local_address, inet_address_view remote_address, sip_callback&& callback) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop_from_handle(sip);

    looper_trace_info(log_module, "opening sip session: loop=%lu, handle=%lu", data.m_handle, sip);

    auto& sip_impl = data.m_sip_sessions[sip];
    sip_impl.open(std::move(local_address), std::move(remote_address), std::move(callback));
}

void listen_for_requests(const sip_session sip, sip::method method, sip_listen_callback&& callback) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop_from_handle(sip);

    looper_trace_info(log_module, "opening sip session: loop=%lu, handle=%lu", data.m_handle, sip);

    auto& sip_impl = data.m_sip_sessions[sip];
    sip_impl.listen(std::move(method), std::move(callback));
}

void request(const sip_session sip, sip::message&& message, sip_request_callback&& callback) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop_from_handle(sip);

    looper_trace_info(log_module, "opening sip session: loop=%lu, handle=%lu", data.m_handle, sip);

    auto& sip_impl = data.m_sip_sessions[sip];
    sip_impl.request(std::move(message), std::move(callback));
}

void send(const sip_session sip, sip::message&& message) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop_from_handle(sip);

    looper_trace_info(log_module, "opening sip session: loop=%lu, handle=%lu", data.m_handle, sip);

    auto& sip_impl = data.m_sip_sessions[sip];
    sip_impl.send(std::move(message));
}

}
