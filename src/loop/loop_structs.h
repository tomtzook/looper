#pragma once

#include <memory>
#include <deque>
#include <mutex>
#include <unordered_map>
#include <condition_variable>
#include <span>
#include <chrono>

#include "looper_types.h"

namespace looper::impl {

struct loop_context;

using resource = handle;

}
