#pragma once

#include <memory>

#include <looper_types.h>
#include "types_internal.h"

namespace looper::os {

// todo: better os layer?
//  maybe with error codes instead
//  and structs instead of classes and oop
//  and platform-independent error codes

using descriptor = int;

namespace event {

struct event;

looper::error create(event** event_out);
void close(event* event);

descriptor get_descriptor(event* event);

looper::error set(event* event);
looper::error clear(event* event);

}

namespace tcp {

struct tcp;

looper::error create(tcp** tcp_out);
void close(tcp* tcp);

descriptor get_descriptor(tcp* tcp);

looper::error get_internal_error(tcp* tcp, looper::error& error_out);

looper::error bind(tcp* tcp, uint16_t port);
looper::error bind(tcp* tcp, std::string_view ip, uint16_t port);

looper::error connect(tcp* tcp, std::string_view ip, uint16_t port);
looper::error finalize_connect(tcp* tcp);

looper::error read(tcp* tcp, uint8_t* buffer, size_t buffer_size, size_t& read_out);
looper::error write(tcp* tcp, const uint8_t* buffer, size_t size, size_t& written_out);

looper::error listen(tcp* tcp, size_t backlog_size);
looper::error accept(tcp* this_tcp, tcp** tcp_out);

}

namespace poll {

struct event_data {
    os::descriptor descriptor;
    event_types events;
};

struct poller;

looper::error create(poller** poller_out);
void close(poller* poller);

looper::error add(poller* poller, os::descriptor descriptor, event_types events);
looper::error set(poller* poller, os::descriptor descriptor, event_types events);
looper::error remove(poller* poller, os::descriptor descriptor);

looper::error poll(poller* poller, size_t max_events, std::chrono::milliseconds timeout, event_data* events, size_t& event_count);

}

}
