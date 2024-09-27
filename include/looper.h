#pragma once

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

}
