
#include <fcntl.h>

#include "linux.h"
#include "os/os.h"

namespace looper::os::file {

struct file {
    os::descriptor fd;
    bool closed;
    size_t offset;
};

static int get_whence(const seek_whence whence) {
    switch (whence) {
        case seek_whence::begin:
            return SEEK_SET;
        case seek_whence::current:
            return SEEK_CUR;
        case seek_whence::end:
            return SEEK_END;
        default:
            return 0;
    }
}

static looper::error get_open_flags(const open_mode mode, const file_attributes attributes, int& flags_out) {
    int flags = 0;

    const bool readable = (mode & open_mode::read) != 0;
    const bool writable = (mode & open_mode::write) != 0;

    if (readable && !writable) {
        flags |= O_RDONLY;
    } else if (!readable && writable) {
        flags |= O_WRONLY;
    } else if (readable && writable) {
        flags |= O_RDWR;
    } else {
        return error_invalid_filemode;
    }

    if ((mode & open_mode::append) != 0) {
        flags |= O_APPEND;
    }
    if ((mode & open_mode::create) != 0) {
        flags |= O_CREAT;
    }
    if ((attributes & file_attributes::directory) != 0) {
        flags |= O_DIRECTORY;
    }

    flags_out = flags;
    return error_success;
}

static looper::error open_file(const std::string_view path, const open_mode mode, const file_attributes attributes, descriptor& descriptor_out) {
    std::string path_c(path);

    int open_flags;
    auto status = get_open_flags(mode, attributes, open_flags);
    if (status != error_success) {
        return status;
    }

    const int fd = ::open(path_c.c_str(), open_flags);
    if (fd < 0) {
        return get_call_error();
    }

    descriptor_out = fd;
    return error_success;
}

looper::error create(file** file_out, const std::string_view path, const open_mode mode, const file_attributes attributes) {
    os::descriptor descriptor;
    auto status = open_file(path, mode, attributes, descriptor);
    if (status != error_success) {
        return status;
    }

    auto* _file = static_cast<file*>(malloc(sizeof(file)));
    if (_file == nullptr) {
        ::close(descriptor);
        return error_allocation;
    }

    _file->fd = descriptor;
    _file->closed = false;
    _file->offset = 0;

    *file_out = _file;
    return error_success;
}

void close(file* file) {
    file->closed = true;
    ::close(file->fd);

    free(file);
}

descriptor get_descriptor(const file* file) {
    return file->fd;
}

looper::error seek(file* file, const size_t offset, const seek_whence whence) {
    if (file->closed) {
        return error_fd_closed;
    }

    const auto result = ::lseek(file->fd, static_cast<off_t>(offset), get_whence(whence));
    if (result < 0) {
        return get_call_error();
    }

    file->offset = result;
    return error_success;
}

looper::error tell(const file* file, size_t& offset_out) {
    if (file->closed) {
        return error_fd_closed;
    }

    offset_out = file->offset;
    return error_success;
}

looper::error read(file* file, uint8_t* buffer, const size_t buffer_size, size_t& read_out) {
    if (file->closed) {
        return error_fd_closed;
    }

    if (buffer_size == 0) {
        read_out = 0;
        return error_success;
    }

    const auto result = ::read(file->fd, buffer, buffer_size);
    if (result == 0) {
        return error_eof;
    }
    if (result < 0) {
        const auto error_code = get_call_error();
        if (error_code == error_again) {
            // while in non-blocking mode, socket operations may return eagain if
            // the operation will end up blocking, as such just return.
            read_out = 0;
            return error_success;
        }

        return get_call_error();
    }

    file->offset += result;
    read_out = result;
    return error_success;
}

looper::error write(file* file, const uint8_t* buffer, const size_t size, size_t& written_out) {
    if (file->closed) {
        return error_fd_closed;
    }

    const auto result = ::write(file->fd, buffer, size);
    if (result < 0) {
        return get_call_error();
    }

    file->offset += result;
    written_out = result;
    return error_success;
}

}
