#pragma once

#include "loop_stream.h"
#include "os/factory.h"

namespace looper::impl {

class file final : public stream {
public:
    file(looper::file handle, loop_context* context, std::string_view path, open_mode mode, file_attributes attributes);

    void seek(size_t offset, seek_whence whence);
    size_t tell() const;

    void close();

protected:
    looper::error read_from_obj(uint8_t *buffer, size_t buffer_size, size_t &read_out) override;
    looper::error write_to_obj(const uint8_t *buffer, size_t size, size_t &written_out) override;

private:
    os::file_ptr m_file_obj;
};

}
