#pragma once

#include <iostream>
#include <span>
#include <cstdint>

namespace looper::util {

class istream_buff final : public std::streambuf {
public:
    explicit istream_buff(const std::span<const uint8_t> buffer) {
        auto* ptr = const_cast<char*>(reinterpret_cast<const char*>(buffer.data()));
        setg(ptr, ptr, ptr + buffer.size());
    }

    pos_type seekoff(const off_type off, const std::ios_base::seekdir dir, std::ios_base::openmode) override {
        if (dir == std::ios_base::cur) {
            gbump(static_cast<int>(off));
        } else if (dir == std::ios_base::end) {
            setg(eback(), egptr() + off, egptr());
        } else if (dir == std::ios_base::beg) {
            setg(eback(), eback() + off, egptr());
        }

        return gptr() - eback();
    }

    pos_type seekpos(const pos_type pos, std::ios_base::openmode) override {
        setg(eback(), eback() + pos, egptr());
        return gptr() - eback();
    }
};

class ostream_buff final : public std::streambuf {
public:
    explicit ostream_buff(const std::span<uint8_t> buffer) {
        auto* ptr = reinterpret_cast<char*>(buffer.data());
        setp(ptr, ptr + buffer.size());
    }

    pos_type seekoff(const off_type off, const std::ios_base::seekdir dir, std::ios_base::openmode) override {
        if (dir == std::ios_base::cur) {
            pbump(static_cast<int>(off));
        } else {
            return pos_type(-1);
        }

        return pptr() - pbase();
    }
};

}
