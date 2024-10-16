#pragma once

#include <string>

#include <looper_types.h>
#include <looper_except.h>

namespace looper {

tcp create_tcp(loop loop);
void destroy_tcp(tcp tcp);

void bind_tcp(tcp tcp, uint16_t port);
void connect_tcp(tcp tcp, std::string_view server_address, uint16_t server_port, tcp_callback&& callback);

void start_tcp_read(tcp tcp, tcp_read_callback&& callback);
void stop_tcp_read(tcp tcp);
void write_tcp(tcp tcp, std::span<const uint8_t> buffer, tcp_callback&& callback);

}
