#pragma once

#include <sip/base.h>
#include <sip/headers.h>
#include <sip/bodies.h>
#include <sip/message.h>
#include <looper_tcp.h>

namespace looper::sip {

using sip_callback = std::function<void(loop, sip_session, error)>;
using sip_request_callback = std::function<void(loop, sip_session, const sip::message*, error)>;

enum class transport {
    tcp
};

sip_session create_sip(loop loop, transport transport);
sip_session create_sip_tcp(loop loop, tcp tcp);
void destroy_sip(sip_session sip);

void open(sip_session sip, inet_address local_address, inet_address remote_address, sip_callback&& callback);
void listen(sip_session sip, sip::method method, sip_request_callback&& callback);
void request(sip_session sip, sip::message&& message, sip_request_callback&& callback);
void send(sip_session sip, sip::message&& message);

}
