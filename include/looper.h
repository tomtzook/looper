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

future execute_on(loop loop, std::chrono::milliseconds delay, loop_callback&& callback);
bool wait_for(future future, std::chrono::milliseconds timeout);

// events
event create_event(loop loop, event_callback&& callback);
void destroy_event(event event);
void set_event(event event);
void clear_event(event event);

// timers
timer create_timer(std::chrono::milliseconds timeout, timer_callback&& callback);
void destroy_timer(timer timer);
void start_timer(timer timer);
void stop_timer(timer timer);
void reset_timer(timer timer);

}
