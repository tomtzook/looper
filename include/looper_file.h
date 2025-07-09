#pragma once

#include <looper_types.h>
#include <looper_except.h>

namespace looper {

file create_file(loop loop, std::string_view path, open_mode mode, file_attributes attributes = file_attributes::none);
void destroy_file(file file);

void seek_file(file file, size_t offset, seek_whence whence);
size_t tell_file(file file);

void start_file_read(file file, read_callback&& callback);
void stop_file_read(file file);
void write_file(file file, std::span<const uint8_t> buffer, write_callback&& callback);

}
