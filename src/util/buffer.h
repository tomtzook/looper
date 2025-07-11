#pragma once

#include <looper_types.h>

namespace looper::util {

class buffer {
public:
    buffer();
    ~buffer();

    [[nodiscard]] size_t pos() const;
    [[nodiscard]] size_t size() const;
    [[nodiscard]] std::span<uint8_t> view(size_t offset = 0, size_t length = 0) const;

    [[nodiscard]] ssize_t find(std::span<const uint8_t> sequence, size_t start = 0) const;

    void seek(size_t pos);
    void truncate_to(size_t pos);
    void write(std::span<const uint8_t> span);

private:
    void resize(size_t new_size);

    uint8_t* m_data;
    size_t m_position;
    size_t m_size;
    size_t m_capacity;
};

}
