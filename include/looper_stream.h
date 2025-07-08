#pragma once

#include <looper_types.h>
#include <looper_except.h>

namespace looper {

void start_read(handle handle, read_callback&& callback);
void stop_read(handle handle);
void write(handle handle, std::span<const uint8_t> buffer, write_callback&& callback);

}
