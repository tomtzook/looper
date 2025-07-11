
#include "buffer.h"

#include <cstring>

namespace looper::util {

static constexpr auto initial_capacity = 1024;

buffer::buffer()
    : m_data(nullptr)
    , m_position(0)
    , m_size(0)
    , m_capacity(0) {
    resize(initial_capacity);
}

buffer::~buffer() {
    if (m_data != nullptr) {
        free(m_data);
    }
}

size_t buffer::pos() const {
    return m_position;
}

size_t buffer::size() const {
    return m_size;
}

std::span<uint8_t> buffer::view(const size_t offset, const size_t length) const {
    if (offset > m_size) {
        throw std::out_of_range("buffer::view->offset");
    }
    if (offset + length > m_size) {
        throw std::out_of_range("buffer::view->length");
    }

    return {m_data + offset, length};
}

ssize_t buffer::find(const std::span<const uint8_t> sequence, const size_t start) const {
    const auto* ptr = m_data + start;
    const auto* seq_ptr = sequence.data();
    auto seq_index = 0;
    for (auto i = start; i < m_size; ++i) {
        if (ptr[i] == seq_ptr[seq_index]) {
            seq_index++;
            if (seq_index == sequence.size()) {
                return static_cast<ssize_t>(i);
            }
        } else {
            seq_index = 0;
        }
    }

    return -1;
}

void buffer::seek(const size_t pos) {
    if (pos > m_size) {
        throw std::out_of_range("buffer::seek");
    }

    m_position = pos;
}

void buffer::truncate_to(const size_t pos) {
    if (pos > m_size) {
        throw std::out_of_range("buffer::truncate_to");
    }

    std::memmove(m_data, m_data + pos, m_size - pos);

    m_position -= pos;
    m_size -= pos;
}

void buffer::write(const std::span<const uint8_t> span) {
    const auto new_position = m_position + span.size();
    if (new_position > m_capacity) {
        resize(new_position);
    }

    std::ranges::copy(span, m_data + m_position);
    m_position = new_position;

    if (new_position > m_size) {
        m_size = new_position;
    }
}

void buffer::resize(const size_t new_size) {
    if (m_data == nullptr) {
        m_data = static_cast<uint8_t*>(malloc(new_size));
    } else {
        m_data = static_cast<uint8_t*>(realloc(m_data, new_size));
    }

    if (m_data == nullptr) {
        throw std::bad_alloc();
    }

    m_capacity = new_size;

    if (m_size > new_size) {
        m_size = new_size;
    }
    if (m_position > new_size) {
        m_position = new_size;
    }
}

}
