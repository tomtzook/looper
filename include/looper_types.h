#pragma once

#include <cstdint>
#include <functional>

namespace looper {

using handle = uint32_t;
static constexpr handle empty_handle = static_cast<handle>(-1);

using loop = handle;

// using resource = handle;
// using timer = handle;
// using event = handle;

using execute_callback = std::function<void(loop loop)>;

}
