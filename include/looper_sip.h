#pragma once

#include <sip/base.h>
#include <sip/headers.h>
#include <sip/bodies.h>
#include <sip/message.h>
#include <looper_tcp.h>

namespace looper::sip {

using sip_callback = std::function<void(loop, sip_session, error)>;
using sip_listen_callback = std::function<void(loop, sip_session, const sip::message*, error)>;
using sip_request_callback = std::function<bool(loop, sip_session, const sip::message*, error)>;

sip_session create_sip(loop loop, transport transport);
sip_session create_sip_tcp(loop loop, tcp tcp);
sip_session create_sip_udp(loop loop, udp udp);
void destroy_sip(sip_session sip);

void open(sip_session sip, inet_address_view local_address, inet_address_view remote_address, sip_callback&& callback);
void listen_for_requests(sip_session sip, sip::method method, sip_listen_callback&& callback);
void request(sip_session sip, sip::message&& message, sip_request_callback&& callback);
void send(sip_session sip, sip::message&& message);

}
