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

void _register_attribute_internal(const std::string& name, std::unique_ptr<_base_attribute_holder_creator> ptr);

template<_attribute_type T>
void register_attribute() {
    const auto name = looper::meta::_header_name<T>::name();
    auto holder = std::make_unique<_attribute_holder_creator<T>>();
    _register_attribute_internal(name, std::move(holder));
}

}

class message {
public:
    message() = default;
    ~message() = default;

    template<fields::_field_type T>
    [[nodiscard]] bool has_field() const;
    template<fields::_field_type T>
    T field() const;
    template<fields::_field_type T>
    std::vector<T> fields() const;
    template<fields::_field_type T>
    void add_field(const T& field);

    template<attributes::_attribute_type T>
    [[nodiscard]] bool has_attribute() const;
    template<attributes::_attribute_type T>
    T attribute() const;
    template<attributes::_attribute_type T>
    std::vector<T> attributes() const;
    template<attributes::_attribute_type T>
    void add_attribute(const T& attribute);

    std::istream& operator>>(std::istream& is);
    std::ostream& operator<<(std::ostream& os);

private:
    void add_field(const std::string& name, std::unique_ptr<fields::_base_field_holder> holder);
    void add_attribute(const std::string& name, std::unique_ptr<attributes::_base_attribute_holder> holder);

    std::map<std::string, std::vector<std::unique_ptr<fields::_base_field_holder>>> m_fields;
    std::map<std::string, std::vector<std::unique_ptr<attributes::_base_attribute_holder>>> m_attributes;
};

template<fields::_field_type T>
bool message::has_field() const {
    const auto name = looper::meta::_header_name<T>::name();
    auto it = m_fields.find(name);
    if (it == m_fields.end()) {
        return false;
    } else {
        return true;
    }
}

template<fields::_field_type T>
T message::field() const {
    const auto name = looper::meta::_header_name<T>::name();
    auto it = m_fields.find(name);
    if (it == m_fields.end()) {
        throw fields::field_not_found();
    }

    if (it->second.size() < 1) {
        throw fields::field_not_found();
    }

    auto holder = reinterpret_cast<fields::_field_holder<T>*>(it->second[0].get());
    return holder->value;
}

template<fields::_field_type T>
std::vector<T> message::fields() const {
    const auto name = looper::meta::_header_name<T>::name();
    auto it = m_fields.find(name);
    if (it == m_fields.end()) {
        throw fields::field_not_found();
    }

    if (it->second.size() < 1) {
        throw fields::field_not_found();
    }

    std::vector<T> result;
    result.reserve(it->second.size());

    for (auto& ptr : it->second) {
        auto holder = reinterpret_cast<fields::_field_holder<T>*>(it->second[0].get());
        result.push_back(holder->value);
    }

    return result;
}

template<fields::_field_type T>
void message::add_field(const T& field) {
    const auto name = looper::meta::_header_name<T>::name();

    auto holder = std::make_unique<fields::_field_holder<T>>();
    holder->value = field;

    add_field(name, std::move(holder));
}

template<attributes::_attribute_type T>
bool message::has_attribute() const {
    const auto name = looper::meta::_header_name<T>::name();
    auto it = m_attributes.find(name);
    if (it == m_attributes.end()) {
        return false;
    } else {
        return true;
    }
}

template<attributes::_attribute_type T>
T message::attribute() const {
    const auto name = looper::meta::_header_name<T>::name();
    auto it = m_attributes.find(name);
    if (it == m_attributes.end()) {
        throw attributes::attribute_not_found();
    }

    if (it->second.size() < 1) {
        throw attributes::attribute_not_found();
    }

    auto holder = reinterpret_cast<attributes::_attribute_holder<T>*>(it->second[0].get());
    return holder->value;
}

template<attributes::_attribute_type T>
std::vector<T> message::attributes() const {
    const auto name = looper::meta::_header_name<T>::name();
    auto it = m_attributes.find(name);
    if (it == m_attributes.end()) {
        throw attributes::attribute_not_found();
    }

    if (it->second.size() < 1) {
        throw attributes::attribute_not_found();
    }

    std::vector<T> result;
    result.reserve(it->second.size());

    for (auto& ptr : it->second) {
        auto holder = reinterpret_cast<attributes::_attribute_holder<T>*>(it->second[0].get());
        result.push_back(holder->value);
    }

    return result;
}

template<attributes::_attribute_type T>
void message::add_attribute(const T& attribute) {
    const auto name = looper::meta::_header_name<T>::name();

    auto holder = std::make_unique<attributes::_attribute_holder<T>>();
    holder->value = attribute;

    add_attribute(name, std::move(holder));
}

}
