
#include <cstring>
#include "session.h"

namespace looper::impl::sip {

#define log_module loop_log_module "_sipsession"

transport::transport(impl::loop_context* context)
    : m_context(context)
    , m_connect_listener()
    , m_data_listener()
    , m_write_callback()
{}

void transport::on_connect(connect_listener&& listener) {
    m_connect_listener = std::move(listener);
}

void transport::on_new_data(data_listener&& listener) {
    m_data_listener = std::move(listener);
}

void transport::on_write_complete(write_callback&& listener) {
    m_write_callback = std::move(listener);
}

tcp_transport::tcp_transport(impl::loop_context* context)
    : transport(context)
    , m_tcp()
{}

tcp_transport::tcp_transport(impl::loop_context* context, std::shared_ptr<impl::tcp> tcp)
    : transport(context)
    , m_tcp(std::move(tcp))
{}

void tcp_transport::open(const inet_address_view local, const inet_address_view remote) {
    m_tcp = std::make_unique<impl::tcp>(0, m_context);
    m_tcp->bind(local.ip, local.port);
    m_tcp->connect(remote.ip, remote.port, [this](looper::loop, looper::tcp, const looper::error error)->void {
        m_connect_listener(error);
    });
}

void tcp_transport::start_reading() {
    m_tcp->start_read([this](looper::loop, looper::handle, const std::span<const uint8_t> data, const looper::error error)->void {
        m_data_listener(data, error);
    });
}

void tcp_transport::send(const std::span<const uint8_t> data) {
    impl::tcp::write_request request;
    const auto buffer_size = data.size_bytes();
    request.buffer = std::unique_ptr<uint8_t[]>(new uint8_t[buffer_size]);
    request.pos = 0;
    request.size = buffer_size;
    request.write_callback = [this](looper::loop, looper::tcp, const looper::error error)->void {
        m_write_callback(error);
    };
    memcpy(request.buffer.get(), data.data(), buffer_size);

    m_tcp->write(std::move(request));
}

void tcp_transport::close() {
    if (m_tcp) {
        m_tcp->close();
        m_tcp.reset();
    }
}

udp_transport::udp_transport(impl::loop_context* context)
    : transport(context)
    , m_udp()
    , m_remote()
{}

udp_transport::udp_transport(impl::loop_context* context, std::shared_ptr<impl::udp> udp)
    : transport(context)
    , m_udp(std::move(udp))
    , m_remote()
{}

void udp_transport::open(const inet_address_view local, const inet_address_view remote) {
    m_udp->bind(local.ip, local.port);
    m_remote = remote;

    m_connect_listener(error_success);
}

void udp_transport::start_reading() {
    m_udp->start_read([this](looper::loop, looper::handle, const inet_address_view& sender, const std::span<const uint8_t> data, const looper::error error)->void {
        m_data_listener(data, error);
    });
}

void udp_transport::send(const std::span<const uint8_t> data) {
    impl::udp::write_request request;
    const auto buffer_size = data.size_bytes();
    request.buffer = std::unique_ptr<uint8_t[]>(new uint8_t[buffer_size]);
    request.pos = 0;
    request.size = buffer_size;
    request.write_callback = [this](looper::loop, looper::tcp, const looper::error error)->void {
        m_write_callback(error);
    };
    request.destination = m_remote;
    memcpy(request.buffer.get(), data.data(), buffer_size);

    m_udp->write(std::move(request));
}

void udp_transport::close() {
    if (m_udp) {
        m_udp->close();
        m_udp.reset();
    }
}

session::session(const sip_session handle, impl::loop_context* context, const looper::sip::transport transport_type)
    : m_handle(handle)
    , m_context(context)
    , m_mutex()
    , m_transport()
    , m_state(state::ready)
    , m_connect_callback()
    , m_request_callback()
    , m_listeners()
    , m_read_buffer()
    , m_in_messages()
    , m_write_buffer() {
    switch (transport_type) {
        case looper::sip::transport::tcp:
            m_transport = std::make_unique<tcp_transport>(context);
            break;
        case looper::sip::transport::udp:
            m_transport = std::make_unique<udp_transport>(context);
            break;
        default:
            throw std::runtime_error("unknown sip transport");
    }
    setup_transport_listeners();
}

session::session(const sip_session handle, impl::loop_context* context, std::shared_ptr<impl::tcp> tcp)
    : m_handle(handle)
    , m_context(context)
    , m_mutex()
    , m_transport()
    , m_state(state::ready)
    , m_connect_callback()
    , m_request_callback()
    , m_listeners()
    , m_read_buffer()
    , m_in_messages()
    , m_write_buffer() {
    switch (tcp->get_state()) {
        case tcp::state::open:
            m_state = state::ready;
            break;
        case tcp::state::connected:
            m_state = state::open;
            break;
        case tcp::state::connecting:
        case tcp::state::closed:
        default:
            throw std::runtime_error("tcp in bad state");
    }

    m_transport = std::make_unique<tcp_transport>(context, std::move(tcp));
    setup_transport_listeners();

    if (m_state == state::open) {
        m_transport->start_reading();
    }
}

session::session(const sip_session handle, impl::loop_context* context, std::shared_ptr<impl::udp> udp)
    : m_handle(handle)
    , m_context(context)
    , m_mutex()
    , m_transport()
    , m_state(state::ready)
    , m_connect_callback()
    , m_request_callback()
    , m_listeners()
    , m_read_buffer()
    , m_in_messages()
    , m_write_buffer() {
    m_transport = std::make_unique<udp_transport>(context, std::move(udp));
    setup_transport_listeners();
}

void session::listen(const looper::sip::method method, looper::sip::sip_request_callback&& callback) {
    std::unique_lock lock(m_mutex);

    m_listeners[method] = std::move(callback);
}

void session::open(inet_address_view local_address, inet_address_view remote_address, looper::sip::sip_callback&& callback) {
    std::unique_lock lock(m_mutex);

    if (m_state != state::ready) {
        throw std::runtime_error("session already open");
    }

    looper_trace_debug(log_module, "session=%lu opening to local=%s:%d, remote=%s:%d",
        m_handle,
        local_address.ip.data(), local_address.port,
        remote_address.ip.data(), remote_address.port);

    m_state = state::opening;
    m_connect_callback = std::move(callback);
    m_transport->open(std::move(local_address), std::move(remote_address));
}

void session::request(looper::sip::message&& message, looper::sip::sip_request_callback&& callback) {
    std::unique_lock lock(m_mutex);

    if (m_state != state::open) {
        throw std::runtime_error("session not ready for request");
    }

    looper_trace_debug(log_module, "session=%lu sending request", m_handle);

    m_state = state::in_transaction;
    m_request_callback = std::move(callback);
    m_transport->send(serialize_message(message));
}

void session::send(looper::sip::message&& message) {
    std::unique_lock lock(m_mutex);

    if (m_state != state::open && m_state != state::in_transaction) {
        throw std::runtime_error("session not ready for sending");
    }

    looper_trace_debug(log_module, "session=%lu sending message", m_handle);

    m_transport->send(serialize_message(message));
}

void session::close() {
    std::unique_lock lock(m_mutex);

    m_transport->close();
    m_transport.reset();
}

void session::setup_transport_listeners() {
    m_transport->on_connect([this](const looper::error error)->void {
        std::unique_lock lock(m_mutex);

        if (error == error_success) {
            m_state = state::open;
            m_transport->start_reading();
        } else {
            m_state = state::errored;
            m_transport.reset();
        }

        if (m_connect_callback != nullptr) {
            invoke_func(lock, "sip_connect_callback", m_connect_callback, m_context->handle, m_handle, error);
            m_connect_callback = nullptr;
        }
    });
    m_transport->on_new_data([this](const std::span<const uint8_t> data, const looper::error error)->void {
        std::unique_lock lock(m_mutex);

        if (error == error_success) {
            m_read_buffer.write(data);
            process_data(lock);
        } else {
            const auto was_in_transaction = m_state == state::in_transaction;
            m_state = state::errored;
            m_transport.reset();

            if (was_in_transaction) {
                const looper::sip::message* message = nullptr;
                invoke_func<>(lock, "sip_transaction_callback", m_request_callback, m_context->handle, m_handle, message, error);
            }
        }
    });
    m_transport->on_write_complete([this](const looper::error error)->void {
        std::unique_lock lock(m_mutex);

        if (error != error_success) {
            const auto was_in_transaction = m_state == state::in_transaction;
            m_state = state::errored;
            m_transport.reset();

            if (was_in_transaction) {
                const looper::sip::message* message = nullptr;
                invoke_func<>(lock, "sip_transaction_callback", m_request_callback, m_context->handle, m_handle, message, error);
            } else {
                looper_trace_error(log_module, "session=%lu received error for writing when not in transaction", m_handle);
            }
        }
    });
}

void session::process_data(std::unique_lock<std::mutex>& lock) {
    if (read_messages()) {
        // we have messages, lets transfer them
        const auto in_transaction = m_state == state::in_transaction;
        while (!m_in_messages.empty()) {
            auto& message = m_in_messages.front();
            if (in_transaction) {
                if (message->is_request()) {
                    // we should receive a response, not request
                    looper_trace_error(log_module, "session=%lu received a request while in transaction", m_handle);
                } else {
                    m_state = state::open;
                    const looper::sip::message* msg = message.get();
                    invoke_func<>(lock, "sip_transaction_callback", m_request_callback, m_context->handle, m_handle, msg, 0);
                }
            } else {
                delegate_to_listeners(lock, message.get());
            }

            m_in_messages.pop_front();
        }
    }
}

bool session::read_messages() {
    static const auto eof_sequence_raw = "\r\n\r\n";
    static const auto eof_sequence = std::span(reinterpret_cast<const uint8_t*>(eof_sequence_raw), strlen(eof_sequence_raw));

    m_read_buffer.seek(0);

    auto start = 0;
    while (start < m_read_buffer.size()) {
        const auto headers_end = m_read_buffer.find(eof_sequence, start);
        if (headers_end < 0) {
            break;
        }

        const auto view = m_read_buffer.view(start, m_read_buffer.size() - start);
        auto msg_ptr = std::make_unique<looper::sip::message>();
        const auto new_pos = looper::sip::read_message(view, *msg_ptr);
        if (new_pos < 0) {
            // not enough data for entire message
            break;
        }

        m_in_messages.push_back(std::move(msg_ptr));
        start = static_cast<size_t>(new_pos);
    }

    if (start > 0) {
        // we found messages
        m_read_buffer.truncate_to(start);
        return true;
    }

    return false;
}

void session::delegate_to_listeners(std::unique_lock<std::mutex>& lock, const looper::sip::message* message) {
    if (message->is_request()) {
        const auto method = message->request_line().method;
        const auto it = m_listeners.find(method);
        if (it != m_listeners.end()) {
            invoke_func<>(lock, "sip_message_callback", it->second, m_context->handle, m_handle, message, 0);
        } else {
            looper_trace_error(log_module, "session=%lu received a request, but no listener for it", m_handle);
        }
    } else {
        looper_trace_error(log_module, "session=%lu received a response while not in transaction", m_handle);
    }
}

std::span<const uint8_t> session::serialize_message(const looper::sip::message& message) {
    const auto size = looper::sip::write_message({m_write_buffer, sizeof(m_write_buffer)}, message);
    if (size < 0) {
        throw std::runtime_error("failed to serialize message");
    }

    return {m_write_buffer, static_cast<size_t>(size)};
}

}
