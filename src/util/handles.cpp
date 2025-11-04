
#include "handles.h"

namespace looper::handles {

handle::handle(const handle_raw raw)
    : m_parent(0)
    , m_type(0)
    , m_index(0) {
    this->raw(raw);
}
handle::handle(const uint8_t parent, const uint8_t type, const uint16_t index)
    : m_parent(parent)
    , m_type(type)
    , m_index(index)
{}

uint8_t handle::parent() const {
    return m_parent;
}

void handle::parent(const uint8_t parent) {
    m_parent = parent;
}

uint8_t handle::type() const {
    return m_type;
}

void handle::type(const uint8_t type) {
    m_type = type;
}

uint16_t handle::index() const {
    return m_index;
}

void handle::index(const uint16_t index) {
    m_index = index;
}

handle_raw handle::raw() const {
    return (m_parent & 0xffff) | ((m_type & 0xffff) << 8) | ((m_index & 0xffffffff) << 16);
}

void handle::raw(const handle_raw raw) {
    m_parent = raw & 0xffff;
    m_type = (raw >> 8) & 0xffff;
    m_index = (raw >> 16) & 0xffffffff;
}

}
