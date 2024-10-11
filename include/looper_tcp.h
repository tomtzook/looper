#pragma once

#include <string>

#include <looper_types.h>
#include <looper_except.h>

namespace looper {

tcp create_tcp(loop loop);
void destroy_tcp(loop loop, tcp tcp);

void bind_tcp(loop loop, tcp tcp, uint16_t port);
void connect_tcp(loop loop, tcp tcp, std::string_view server_address, uint16_t server_port, tcp_callback&& callback);

void start_tcp_read(loop loop, tcp tcp, tcp_read_callback&& callback);
void write_tcp(loop loop, tcp tcp, std::span<const uint8_t> buffer, tcp_callback&& callback);

}
