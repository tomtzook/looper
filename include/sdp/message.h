#pragma once

#include <sdp/fields.h>
#include <sdp/attributes.h>

namespace looper::sdp {

namespace fields {

void _register_field_internal(const std::string& name, std::unique_ptr<_base_field_holder_creator> ptr);

template<_field_type T>
void register_field() {
    const auto name = looper::meta::_header_name<T>::name();
    auto holder = std::make_unique<_field_holder_creator<T>>();
    _register_field_internal(name, std::move(holder));
}

}

namespace attributes {

void _register_named_attribute_internal(const std::string& name, std::unique_ptr<_base_attribute_holder_creator> ptr);
void _register_unnamed_attribute_internal(const std::string& name, std::unique_ptr<_base_unnamed_attribute_holder_creator> ptr);

template<_attribute_type T>
void register_attribute() {
    const auto name = looper::meta::_header_name<T>::name();
    if constexpr (std::is_base_of_v<attributes::_unnamed_attribute, T>) {
        auto holder = std::make_unique<_unnamed_attribute_holder_creator<T>>();
        _register_unnamed_attribute_internal(name, std::move(holder));
    } else {
        auto holder = std::make_unique<_named_attribute_holder_creator<T>>();
        _register_named_attribute_internal(name, std::move(holder));
    }
}

}

class message {
public:
    message() = default;
    message(const message&) = delete;
    message(message&&) = default;
    ~message() = default;

    template<fields::_field_type T>
    [[nodiscard]] bool has_field() const;
    template<fields::_field_type T>
    T field() const;
    template<fields::_field_type T>
    std::vector<T> fields() const;
    template<fields::_field_type T>
    void add_field(const T& field);
    template<fields::_field_type T>
    void add_field(T&& field);

    template<attributes::_attribute_type T>
    [[nodiscard]] bool has_attribute() const;
    template<attributes::_attribute_type T>
    T attribute() const;
    template<attributes::_attribute_type T>
    std::vector<T> attributes() const;
    template<attributes::_attribute_type T>
    void add_attribute(const T& attribute);
    template<attributes::_attribute_type T>
    void add_attribute(T&& attribute);

    std::istream& operator>>(std::istream& is);
    std::ostream& operator<<(std::ostream& os);

private:
    static const std::string unnamed_attr_generic_name;

    void add_field(const std::string& name, std::unique_ptr<fields::_base_field_holder> holder);
    void add_named_attribute(const std::string& name, std::unique_ptr<attributes::_base_attribute_holder> holder);
    void add_unnamed_attribute(const std::string& name, std::unique_ptr<attributes::_base_attribute_holder> holder);

    std::map<std::string, std::vector<std::unique_ptr<fields::_base_field_holder>>> m_fields;

    using attr_map = std::map<std::string, std::vector<std::unique_ptr<attributes::_base_attribute_holder>>>;
    attr_map m_named_attributes;
    attr_map m_unnamed_attributes;
};

template<fields::_field_type T>
bool message::has_field() const {
    const auto name = looper::meta::_header_name<T>::name();

    const auto it = m_fields.find(name);
    if (it == m_fields.end()) {
        return false;
    } else {
        return true;
    }
}

template<fields::_field_type T>
T message::field() const {
    const auto name = looper::meta::_header_name<T>::name();

    const auto it = m_fields.find(name);
    if (it == m_fields.end()) {
        throw fields::field_not_found();
    }

    if (it->second.empty()) {
        throw fields::field_not_found();
    }

    auto holder = reinterpret_cast<fields::_field_holder<T>*>(it->second[0].get());
    return holder->value;
}

template<fields::_field_type T>
std::vector<T> message::fields() const {
    const auto name = looper::meta::_header_name<T>::name();

    const auto it = m_fields.find(name);
    if (it == m_fields.end()) {
        throw fields::field_not_found();
    }

    if (it->second.empty()) {
        throw fields::field_not_found();
    }

    std::vector<T> result;
    result.reserve(it->second.size());

    for (auto& ptr : it->second) {
        auto holder = reinterpret_cast<fields::_field_holder<T>*>(ptr.get());
        result.push_back(holder->value);
    }

    return result;
}

template<fields::_field_type T>
void message::add_field(const T& field) {
    T copy = field;
    add_field(std::move(copy));
}

template<fields::_field_type T>
void message::add_field(T&& field) {
    std::string name;
    if constexpr (std::is_same_v<T, sdp::fields::generic_field>) {
        name = field.name;
    } else {
        name = looper::meta::_header_name<T>::name();
    }

    auto holder = std::make_unique<fields::_field_holder<T>>();
    holder->value = std::forward<T>(field);

    add_field(name, std::move(holder));
}

template<attributes::_attribute_type T>
bool message::has_attribute() const {
    const auto name = looper::meta::_header_name<T>::name();

    const attr_map* map;
    if constexpr (std::is_base_of_v<attributes::_unnamed_attribute, T>) {
        map = &m_unnamed_attributes;
    } else {
        map = &m_named_attributes;
    }

    const auto it = map->find(name);
    if (it == map->end()) {
        return false;
    } else {
        return true;
    }
}

template<attributes::_attribute_type T>
T message::attribute() const {
    const auto name = looper::meta::_header_name<T>::name();

    const attr_map* map;
    if constexpr (std::is_base_of_v<attributes::_unnamed_attribute, T>) {
        map = &m_unnamed_attributes;
    } else {
        map = &m_named_attributes;
    }

    const auto it = map->find(name);
    if (it == map->end()) {
        throw attributes::attribute_not_found();
    }

    if (it->second.empty()) {
        throw attributes::attribute_not_found();
    }

    auto holder = reinterpret_cast<attributes::_attribute_holder<T>*>(it->second[0].get());
    return holder->value;
}

template<attributes::_attribute_type T>
std::vector<T> message::attributes() const {
    const auto name = looper::meta::_header_name<T>::name();

    const attr_map* map;
    if constexpr (std::is_base_of_v<attributes::_unnamed_attribute, T>) {
        map = &m_unnamed_attributes;
    } else {
        map = &m_named_attributes;
    }

    const auto it = map->find(name);
    if (it == map->end()) {
        throw attributes::attribute_not_found();
    }

    if (it->second.empty()) {
        throw attributes::attribute_not_found();
    }

    std::vector<T> result;
    result.reserve(it->second.size());

    for (auto& ptr : it->second) {
        auto holder = reinterpret_cast<attributes::_attribute_holder<T>*>(ptr.get());
        result.push_back(holder->value);
    }

    return result;
}

template<attributes::_attribute_type T>
void message::add_attribute(const T& attribute) {
    T copy = attribute;
    add_attribute(std::move(copy));
}

template<attributes::_attribute_type T>
void message::add_attribute(T&& attribute) {
    std::string name;
    if constexpr (std::is_same_v<T, sdp::attributes::generic_named_attribute>) {
        name = attribute.name;
    } else if constexpr (std::is_same_v<T, sdp::attributes::generic_unnamed_attribute>) {
        name = unnamed_attr_generic_name;
    } else {
        name = looper::meta::_header_name<T>::name();
    }

    auto holder = std::make_unique<attributes::_attribute_holder<T>>();
    holder->value = std::forward<T>(attribute);

    if constexpr (std::is_base_of_v<attributes::_unnamed_attribute, T>) {
        add_unnamed_attribute(name, std::move(holder));
    } else {
        add_named_attribute(name, std::move(holder));
    }
}

}
