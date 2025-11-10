
#include "loop.h"
#include "loop_udp.h"

namespace looper::impl {

#define log_module loop_log_module "_udp"

udp_io::udp_io()
    : m_obj(os::udp::create())
{}

os::descriptor udp_io::get_descriptor() const {
    return os::get_descriptor(m_obj);
}

looper::error udp_io::read(udp_read_data& data) const {
    char ip_buff[64]{};
    uint16_t port;
    const auto error = os::interface::udp::read(
        m_obj,
        data.buffer.data(),
        data.buffer.size(),
        data.read_count,
        ip_buff,
        sizeof(ip_buff),
        port);
    if (error != error_success) {
        return error;
    }

    data.sender = inet_address_view{std::string_view(ip_buff), port};
    return error_success;
}

looper::error udp_io::write(const udp_write_request& request, size_t& written) const {
    return os::interface::udp::write(
                m_obj,
                request.destination.ip,
                request.destination.port,
                request.buffer.get() + request.pos,
                request.size - request.pos,
                written);
}

void udp_io::close() {
    m_obj.close();
}

udp::udp(const looper::udp handle, const loop_ptr& loop)
    : m_io(handle, loop, udp_io())
{}

void udp::bind(const uint16_t port) {
    auto [lock, control] = m_io.use();
    control.state.verify_not_errored();

    OS_CHECK_THROW(os::ipv4_bind(m_io.io_obj().m_obj, port));
}

void udp::bind(const std::string_view address, const uint16_t port) {
    auto [lock, control] = m_io.use();
    control.state.verify_not_errored();

    OS_CHECK_THROW(os::ipv4_bind(m_io.io_obj().m_obj, address, port));
}

void udp::start_read(udp_read_callback&& callback) {
    m_io.start_read([callback](const looper::handle handle, const udp_read_data& data)->void {
        callback(handle, data.sender, data.buffer, data.error);
    });
}

void udp::stop_read() {
    m_io.stop_read();
}

void udp::write(udp_write_request&& request) {
    m_io.write(std::move(request));
}

void udp::close() {
    m_io.close();
}

}
