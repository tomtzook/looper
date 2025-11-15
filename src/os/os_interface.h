#pragma once

#include <memory>

#include <looper_types.h>
#include "types_internal.h"

namespace looper::os {

using descriptor = int;

namespace interface {

namespace event {

struct event;

[[nodiscard]] looper::error create(event** event_out) noexcept;
void close(const event* event) noexcept;

[[nodiscard]] descriptor get_descriptor(const event* event) noexcept;

[[nodiscard]] looper::error set(const event* event) noexcept;
[[nodiscard]] looper::error clear(const event* event) noexcept;

}

namespace tcp {

struct tcp;

[[nodiscard]] looper::error create(tcp** tcp_out) noexcept;
void close(tcp* tcp) noexcept;

[[nodiscard]] descriptor get_descriptor(const tcp* tcp) noexcept;

[[nodiscard]] looper::error get_internal_error(const tcp* tcp, looper::error& error_out) noexcept;

[[nodiscard]] looper::error bind(const tcp* tcp, uint16_t port) noexcept;
[[nodiscard]] looper::error bind(const tcp* tcp, std::string_view ip, uint16_t port) noexcept;

[[nodiscard]] looper::error connect(tcp* tcp, std::string_view ip, uint16_t port) noexcept;
[[nodiscard]] looper::error finalize_connect(tcp* tcp) noexcept;

[[nodiscard]] looper::error read(const tcp* tcp, uint8_t* buffer, size_t buffer_size, size_t& read_out) noexcept;
[[nodiscard]] looper::error write(const tcp* tcp, const uint8_t* buffer, size_t size, size_t& written_out) noexcept;

[[nodiscard]] looper::error listen(const tcp* tcp, size_t backlog_size) noexcept;
[[nodiscard]] looper::error accept(const tcp* this_tcp, tcp** tcp_out) noexcept;

}

#ifdef LOOPER_UNIX_SOCKETS

namespace unix_sock {

struct unix_socket;

[[nodiscard]] looper::error create(unix_socket** skt_out) noexcept;
void close(unix_socket* skt) noexcept;

[[nodiscard]] descriptor get_descriptor(const unix_socket* skt) noexcept;

[[nodiscard]] looper::error bind(const unix_socket* skt, std::string_view path) noexcept;

[[nodiscard]] looper::error connect(unix_socket* skt, std::string_view path) noexcept;
[[nodiscard]] looper::error finalize_connect(unix_socket* skt) noexcept;

[[nodiscard]] looper::error read(const unix_socket* skt, uint8_t* buffer, size_t buffer_size, size_t& read_out) noexcept;
[[nodiscard]] looper::error write(const unix_socket* skt, const uint8_t* buffer, size_t size, size_t& written_out) noexcept;

[[nodiscard]] looper::error listen(const unix_socket* skt, size_t backlog_size) noexcept;
[[nodiscard]] looper::error accept(const unix_socket* this_skt, unix_socket** skt_out) noexcept;

}

#endif

namespace udp {

struct udp;

[[nodiscard]] looper::error create(udp** udp_out) noexcept;
void close(udp* udp) noexcept;

[[nodiscard]] descriptor get_descriptor(const udp* udp) noexcept;

[[nodiscard]] looper::error get_internal_error(const udp* udp, looper::error& error_out) noexcept;

[[nodiscard]] looper::error bind(const udp* udp, uint16_t port) noexcept;
[[nodiscard]] looper::error bind(const udp* udp, std::string_view ip, uint16_t port) noexcept;

[[nodiscard]] looper::error read(const udp* udp, uint8_t* buffer, size_t buffer_size, size_t& read_out, char* sender_ip_buff, size_t sender_ip_buff_size, uint16_t& sender_port_out) noexcept;
[[nodiscard]] looper::error write(const udp* udp, std::string_view dest_ip, uint16_t dest_port, const uint8_t* buffer, size_t size, size_t& written_out) noexcept;

}

namespace file {

enum class open_mode {
    read = 1,
    write = 2,
    append = 4,
    create = 8,
};

enum class file_attributes {
    none = 0,
    directory = 1
};

enum class seek_whence {
    begin = 0,
    current = 1,
    end = 2
};

struct file;

[[nodiscard]] looper::error create(file** file_out, std::string_view path, open_mode mode, file_attributes attributes) noexcept;
void close(file* file) noexcept;

[[nodiscard]] descriptor get_descriptor(const file* file) noexcept;

[[nodiscard]] looper::error seek(file* file, size_t offset, seek_whence whence) noexcept;
[[nodiscard]] looper::error tell(const file* file, size_t& offset_out) noexcept;

[[nodiscard]] looper::error read(file* file, uint8_t* buffer, size_t buffer_size, size_t& read_out) noexcept;
[[nodiscard]] looper::error write(file* file, const uint8_t* buffer, size_t size, size_t& written_out) noexcept;

}

namespace poll {

struct event_data {
    os::descriptor descriptor;
    event_type events;
};

struct poller;

[[nodiscard]] looper::error create(poller** poller_out) noexcept;
void close(const poller* poller) noexcept;

[[nodiscard]] looper::error add(const poller* poller, os::descriptor descriptor, event_type events) noexcept;
[[nodiscard]] looper::error set(const poller* poller, os::descriptor descriptor, event_type events) noexcept;
[[nodiscard]] looper::error remove(const poller* poller, os::descriptor descriptor) noexcept;

[[nodiscard]] looper::error poll(poller* poller, size_t max_events, std::chrono::milliseconds timeout, event_data* events, size_t& event_count) noexcept;

}

};

}
