#pragma once

#include <span>

#include "looper_types.h"

#include "loop_structs.h"

namespace looper::impl {

loop_context* create_loop();
void destroy_loop(loop_context* context);

// event
void add_event(loop_context* context, event_data* event);
void remove_event(loop_context* context, event_data* event);

// timer
void add_timer(loop_context* context, timer_data* timer);
void remove_timer(loop_context* context, timer_data* timer);
void reset_timer(loop_context* context, timer_data* timer);

// execute
void add_future(loop_context* context, future_data* future);
void remove_future(loop_context* context, future_data* future);
void exec_future(loop_context* context, future_data* future);

// tcp
void add_tcp(loop_context* context, tcp_data* tcp);
void remove_tcp(loop_context* context, tcp_data* tcp);
void connect_tcp(loop_context* context, tcp_data* tcp, std::string_view server_address, uint16_t server_port);
void start_tcp_read(loop_context* context, tcp_data* tcp);
void stop_tcp_read(loop_context* context, tcp_data* tcp);
void write_tcp(loop_context* context, tcp_data* tcp, tcp_data::write_request&& request);

void add_tcp_server(loop_context* context, tcp_server_data* tcp);
void remove_tcp_server(loop_context* context, tcp_server_data* tcp);

// run
bool run_once(loop_context* context);

}
