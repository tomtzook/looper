#pragma once

#include <cstddef>
#include <memory>
#include <iterator>

#include <looper_types.h>
#include <looper_except.h>

namespace looper::handles {

using handle_raw = looper::handle;
static_assert(sizeof(handle_raw) >= sizeof(uint32_t));

enum handle_types : uint8_t {
    type_loop = 0,
    type_resource,
    type_event,
    type_timer,
    type_future,
    type_tcp,
    type_tcp_server,
    type_udp,
    type_max
};

struct handle {
public:
    explicit handle(const handle_raw raw)
        : m_parent(0)
        , m_type(0)
        , m_index(0) {
        this->raw(raw);
    }
    handle(const uint8_t parent, const uint8_t type, const uint16_t index)
        : m_parent(parent)
        , m_type(type)
        , m_index(index)
    {}

    [[nodiscard]] inline uint8_t parent() const {
        return m_parent;
    }

    inline void parent(const uint8_t parent) {
        m_parent = parent;
    }

    [[nodiscard]] inline uint8_t type() const {
        return m_type;
    }

    inline void type(const uint8_t type) {
        m_type = type;
    }

    [[nodiscard]] inline uint16_t index() const {
        return m_index;
    }

    inline void index(const uint16_t index) {
        m_index = index;
    }

    [[nodiscard]] inline handle_raw raw() const {
        return (m_parent & 0xffff) | ((m_type & 0xffff) << 8) | ((m_index & 0xffffffff) << 16);
    }

    inline void raw(const handle_raw raw) {
        m_parent = raw & 0xffff;
        m_type = (raw >> 8) & 0xffff;
        m_index = (raw >> 16) & 0xffffffff;
    }

private:
    uint8_t m_parent;
    uint8_t m_type;
    uint16_t m_index;
};

template<typename type_, size_t capacity_>
class handle_table {
public:
    static_assert(capacity_ < UINT16_MAX);
    static constexpr size_t capacity = capacity_ - 1;

    struct iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using difference_type   = std::ptrdiff_t;
        using value_type        = type_;
        using pointer           = value_type*;
        using reference         = value_type&;

        iterator(handle_table& table, std::shared_ptr<value_type>* ptr, size_t index)
            : m_table(table)
            , m_ptr(ptr)
            , m_index(index) {

            if (!m_ptr[m_index] && m_index < capacity) {
                iterate_to_next_element();
            }
        }

        std::pair<handle_raw, reference> operator*() const {
            handle handle(m_table.m_parent, m_table.m_type, m_index);
            auto data = m_ptr[m_index].get();
            return {handle.raw(), *data};
        }

        iterator& operator++() {
            iterate_to_next_element();
            return *this;
        }

        iterator operator++(int) {
            iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        friend bool operator== (const iterator& a, const iterator& b) {
            return a.m_index == b.m_index;
        }
        friend bool operator!= (const iterator& a, const iterator& b) {
            return a.m_index != b.m_index;
        }

    private:
        void iterate_to_next_element() {
            do {
                m_index++;
            } while (!m_ptr[m_index] && m_index < capacity);
        }

        handle_table& m_table;
        std::shared_ptr<value_type>* m_ptr;
        size_t m_index;
    };

    handle_table(const uint8_t parent, const uint8_t type)
        : m_parent(parent)
        , m_type(type)
        , m_data()
        , m_count(0)
    {}

    [[nodiscard]] bool empty() const {
        return m_count < 1;
    }

    [[nodiscard]] bool has(const handle_raw handle_raw) const {
        if (handle_raw == empty_handle) {
            return false;
        }

        const handles::handle handle(handle_raw);
        if (handle.parent() != m_parent || handle.type() != m_type || handle.index() >= capacity) {
            return false;
        }

        if (m_data[handle.index()]) {
            return true;
        } else {
            return false;
        }
    }

    const type_& operator[](handle_raw handle_raw) const {
        const auto handle = verify_handle(handle_raw);

        const auto index = handle.index();
        return *m_data[index].get();
    }

    type_& operator[](const handle_raw handle_raw) {
        const auto handle = verify_handle(handle_raw);

        const auto index = handle.index();
        return *m_data[index].get();
    }

    std::shared_ptr<type_> share(const handle_raw handle_raw) {
        const auto handle = verify_handle(handle_raw);

        const auto index = handle.index();
        return m_data[index];
    }

    template<typename... arg_>
    std::pair<handle_raw, std::shared_ptr<type_>> allocate_new(arg_&&... args) {
        const auto spot = find_next_available_spot();
        if (spot < 0) {
            throw no_space_exception();
        }

        const auto index = static_cast<uint16_t>(spot);
        handle handle(m_parent, m_type, index);
        const auto handle_raw = handle.raw();

        auto data = std::make_unique<type_>(handle_raw, std::forward<arg_>(args)...);
        return {handle_raw, std::move(data)};
    }

    template<typename... arg_>
    std::pair<handle_raw, type_&> assign_new(arg_&&... args) {
        auto [handle, data] = this->allocate_new(std::forward<arg_>(args)...);
        return assign(handle, std::move(data));
    }

    [[nodiscard]] handle_raw reserve() const {
        const auto spot = find_next_available_spot();
        if (spot < 0) {
            throw no_space_exception();
        }

        const auto index = static_cast<uint16_t>(spot);
        const handle handle(m_parent, m_type, index);
        return  handle.raw();
    }

    std::pair<handle_raw, type_&> assign(const handle_raw new_handle, std::shared_ptr<type_>&& ptr) {
        const auto handle = valid_handle_for_us(new_handle);
        auto index = handle.index();

        if (m_data[index]) {
            throw no_space_exception();
        }

        m_data[index] = std::move(ptr);
        m_count++;

        auto data = m_data[index].get();
        return {handle.raw(), reinterpret_cast<type_&>(*data)};
    }

    std::shared_ptr<type_> release(const handle_raw handle_raw) {
        const auto handle = verify_handle(handle_raw);

        const auto index = handle.index();

        std::shared_ptr<type_> data;
        m_data[index].swap(data);
        m_count--;

        return std::move(data);
    }

    iterator begin() {
        return iterator(*this, &m_data[0], 0);
    }

    iterator end()   {
        return iterator(*this, &m_data[capacity], capacity);
    }

private:
    [[nodiscard]] ssize_t find_next_available_spot() const {
        for (int i = 0; i < capacity; ++i) {
            if (!m_data[i]) {
                return i;
            }
        }

        return -1;
    }

    handles::handle verify_handle(const handle_raw handle_raw) {
        const auto handle = valid_handle_for_us(handle_raw);

        if (!m_data[handle.index()]) {
            throw no_such_handle_exception(handle_raw);
        }

        return handle;
    }

    handles::handle valid_handle_for_us(const handle_raw handle_raw) {
        const handles::handle handle(handle_raw);

        if (handle.parent() != m_parent || handle.type() != m_type || handle.index() >= capacity) {
            throw bad_handle_exception(handle_raw);
        }

        return handle;
    }

    uint8_t m_parent;
    uint8_t m_type;
    std::shared_ptr<type_> m_data[capacity];
    size_t m_count;
};

}