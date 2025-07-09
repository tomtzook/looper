#pragma once

#include <span>

#include "looper_types.h"

#include "loop_structs.h"

namespace looper::impl {

loop_context* create_loop(looper::loop handle);
void destroy_loop(loop_context* context);

bool run_once(loop_context* context);

}
