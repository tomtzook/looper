#pragma once

#include <looper_types.h>
#include <looper_sip.h>

#include "../loop/loop_internal.h"
#include "../loop/loop_tcp.h"
#include "../loop/loop_udp.h"
#include "../util/buffer.h"

namespace looper::impl::sip {

class transport {
public:
    using connect_listener = std::function<void(looper::error)>;
    using data_listener = std::function<void(std::span<const uint8_t>, looper::error)>;
    using write_callback = std::function<void(looper::error)>;

    explicit transport(impl::loop_context* context);
    virtual ~transport() = default;

    void on_connect(connect_listener&& listener);
    void on_new_data(data_listener&& listener);
    void on_write_complete(write_callback&& listener);

    virtual void open(inet_address local, inet_address remote) = 0;
    virtual void start_reading() = 0;
    virtual void send(std::span<const uint8_t> data) = 0;
    virtual void close() = 0;

protected:
    impl::loop_context* m_context;
    connect_listener m_connect_listener;
    data_listener m_data_listener;
    write_callback m_write_callback;
};

class tcp_transport final : public transport {
public:
    explicit tcp_transport(impl::loop_context* context);
    tcp_transport(impl::loop_context* context, std::shared_ptr<impl::tcp> tcp);

    void open(inet_address local, inet_address remote) override;
    void start_reading() override;
    void send(std::span<const uint8_t> data) override;
    void close() override;

private:
    std::shared_ptr<tcp> m_tcp;
};

class session {
public:
    enum class state {
        ready,
        opening,
        open,
        in_transaction,
        errored
    };

    session(sip_session handle, impl::loop_context* context, looper::sip::transport transport_type);
    session(sip_session handle, impl::loop_context* context, std::shared_ptr<impl::tcp> tcp);

    void listen(looper::sip::method method, looper::sip::sip_request_callback&& callback);

    void open(inet_address local_address, inet_address remote_address, looper::sip::sip_callback&& callback);
    void request(looper::sip::message&& message, looper::sip::sip_request_callback&& callback);
    void send(looper::sip::message&& message);
    void close();

private:
    void setup_transport_listeners();
    void process_data(std::unique_lock<std::mutex>& lock);
    bool read_messages();
    void delegate_to_listeners(std::unique_lock<std::mutex>& lock, const looper::sip::message* message);
    std::span<const uint8_t> serialize_message(const looper::sip::message& message);

    sip_session m_handle;
    impl::loop_context* m_context;

    std::mutex m_mutex;
    std::unique_ptr<transport> m_transport;
    state m_state;
    looper::sip::sip_callback m_connect_callback;
    looper::sip::sip_request_callback m_request_callback;
    std::map<looper::sip::method, looper::sip::sip_request_callback> m_listeners;

    util::buffer m_read_buffer;
    std::deque<std::unique_ptr<looper::sip::message>> m_in_messages;
    uint8_t m_write_buffer[1024];
};

}
