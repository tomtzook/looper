#pragma once

#include <memory>

#include <looper_types.h>
#include "types_internal.h"

namespace looper::os {

using descriptor = int;

namespace event {

struct event;

looper::error create(event** event_out);
void close(event* event);

descriptor get_descriptor(const event* event);

looper::error set(const event* event);
looper::error clear(const event* event);

}

namespace tcp {

struct tcp;

looper::error create(tcp** tcp_out);
void close(tcp* tcp);

descriptor get_descriptor(const tcp* tcp);

looper::error get_internal_error(const tcp* tcp, looper::error& error_out);

looper::error bind(const tcp* tcp, uint16_t port);
looper::error bind(const tcp* tcp, std::string_view ip, uint16_t port);

looper::error connect(tcp* tcp, std::string_view ip, uint16_t port);
looper::error finalize_connect(tcp* tcp);

looper::error read(const tcp* tcp, uint8_t* buffer, size_t buffer_size, size_t& read_out);
looper::error write(const tcp* tcp, const uint8_t* buffer, size_t size, size_t& written_out);

looper::error listen(const tcp* tcp, size_t backlog_size);
looper::error accept(const tcp* this_tcp, tcp** tcp_out);

}

namespace udp {

struct udp;

looper::error create(udp** udp_out);
void close(udp* udp);

descriptor get_descriptor(const udp* udp);

looper::error get_internal_error(const udp* udp, looper::error& error_out);

looper::error bind(const udp* udp, uint16_t port);
looper::error bind(const udp* udp, std::string_view ip, uint16_t port);

looper::error read(const udp* udp, uint8_t* buffer, size_t buffer_size, size_t& read_out, char* sender_ip_buff, size_t sender_ip_buff_size, uint16_t& sender_port_out);
looper::error write(const udp* udp, std::string_view dest_ip, uint16_t dest_port, const uint8_t* buffer, size_t size, size_t& written_out);

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

looper::error create(file** file_out, std::string_view path, open_mode mode, file_attributes attributes);
void close(file* file);

descriptor get_descriptor(const file* file);

looper::error seek(file* file, size_t offset, seek_whence whence);
looper::error tell(const file* file, size_t& offset_out);

looper::error read(file* file, uint8_t* buffer, size_t buffer_size, size_t& read_out);
looper::error write(file* file, const uint8_t* buffer, size_t size, size_t& written_out);

}

namespace poll {

struct event_data {
    os::descriptor descriptor;
    event_types events;
};

struct poller;

looper::error create(poller** poller_out);
void close(poller* poller);

looper::error add(const poller* poller, os::descriptor descriptor, event_types events);
looper::error set(const poller* poller, os::descriptor descriptor, event_types events);
looper::error remove(const poller* poller, os::descriptor descriptor);

looper::error poll(poller* poller, size_t max_events, std::chrono::milliseconds timeout, event_data* events, size_t& event_count);

}

}
