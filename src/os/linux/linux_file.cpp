
#include <fcntl.h>
#include <new>

#include "linux.h"
#include "os/os_interface.h"

namespace looper::os::interface::file {

static open_mode operator|(open_mode lhs, open_mode rhs) {
    return static_cast<open_mode>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

static open_mode operator&(open_mode lhs, open_mode rhs) {
    return static_cast<open_mode>(static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs));
}

static bool operator!=(open_mode lhs, uint32_t rhs) {
    return static_cast<uint32_t>(lhs) != rhs;
}

static file_attributes operator|(file_attributes lhs, file_attributes rhs) {
    return static_cast<file_attributes>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

static file_attributes operator&(file_attributes lhs, file_attributes rhs) {
    return static_cast<file_attributes>(static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs));
}

static bool operator!=(file_attributes lhs, uint32_t rhs) {
    return static_cast<uint32_t>(lhs) != rhs;
}

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
        flags |= (O_CREAT | O_TRUNC);
    }
    if ((attributes & file_attributes::directory) != 0) {
        flags |= O_DIRECTORY;
    }

    flags_out = flags;
    return error_success;
}

static looper::error open_file(
    const std::string_view path,
    const open_mode mode,
    const file_attributes attributes,
    descriptor& descriptor_out) {
    const std::string path_c(path);

    int open_flags;
    const auto status = get_open_flags(mode, attributes, open_flags);
    if (status != error_success) {
        return status;
    }

    static constexpr auto default_perms = S_IRWXU;
    const int fd = ::open(path_c.c_str(), open_flags, default_perms);
    if (fd < 0) {
        return get_call_error();
    }

    descriptor_out = fd;
    return error_success;
}

looper::error create(
    file** file_out,
    const std::string_view path,
    const open_mode mode,
    const file_attributes attributes) {
    os::descriptor descriptor;
    const auto status = open_file(path, mode, attributes, descriptor);
    if (status != error_success) {
        return status;
    }

    auto* _file = new (std::nothrow) file;
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

    delete file;
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

    size_t read_count;
    const auto status = io_read(file->fd, buffer, buffer_size, read_count);
    if (status != error_success) {
        return status;
    }

    file->offset += read_count;
    read_out = read_count;
    return error_success;
}

looper::error write(file* file, const uint8_t* buffer, const size_t size, size_t& written_out) {
    if (file->closed) {
        return error_fd_closed;
    }

    size_t written;
    const auto status = io_write(file->fd, buffer, size, written);
    if (status != error_success) {
        return status;
    }

    file->offset += written;
    written_out = written;
    return error_success;
}

}
