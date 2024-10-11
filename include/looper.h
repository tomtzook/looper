#pragma once

#include <chrono>

#include <looper_types.h>
#include <looper_except.h>
#include <looper_trace.h>

namespace looper {

loop create();
void destroy(loop loop);

void run_once(loop loop);
void run_forever(loop loop);

void execute_on(loop loop, execute_callback&& callback);
void execute_on_and_wait(loop loop, execute_callback&& callback);

// events
event create_event(loop loop, event_callback&& callback);
void destroy_event(loop loop, event event);
void set_event(loop loop, event event);
void clear_event(loop loop, event event);

// timers
timer create_timer(loop loop, std::chrono::milliseconds timeout, timer_callback&& callback);
void destroy_timer(loop loop, timer timer);
void start_timer(loop loop, timer timer);
void stop_timer(loop loop, timer timer);
void reset_timer(loop loop, timer timer);

}
