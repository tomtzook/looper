
#include <ranges>
#include <sdp/message.h>

namespace looper::sdp {

namespace fields {

static std::unordered_map<std::string, std::unique_ptr<_base_field_holder_creator>>& _get_holders() {
    static std::unordered_map<std::string, std::unique_ptr<_base_field_holder_creator>> _field_creators;
    return _field_creators;
}

void __attribute__((constructor)) register_known_fields() {
    register_field<sdp_version>();
    register_field<session_name>();
    register_field<originator>();
    register_field<connection_info>();
    register_field<media_description>();
    register_field<time_description>();
    register_field<bandwidth_information>();
}

void _register_field_internal(const std::string& name, std::unique_ptr<_base_field_holder_creator> ptr) {
    _get_holders()[name] = std::move(ptr);
}

std::optional<std::unique_ptr<_base_field_holder>> _create_field(const std::string& name) {
    const auto it = _get_holders().find(name);
    if (it != _get_holders().end()) {
        return it->second->create();
    } else {
        return std::nullopt;
    }
}

}

namespace attributes {

static std::unordered_map<std::string, std::unique_ptr<_base_attribute_holder_creator>>& _get_named_holders() {
    static std::unordered_map<std::string, std::unique_ptr<_base_attribute_holder_creator>> _attribute_creators;
    return _attribute_creators;
}

static std::unordered_map<std::string, std::unique_ptr<_base_unnamed_attribute_holder_creator>>& _get_unnamed_holders() {
    static std::unordered_map<std::string, std::unique_ptr<_base_unnamed_attribute_holder_creator>> _attribute_creators;
    return _attribute_creators;
}

void __attribute__((constructor)) register_known_attributes() {
    register_attribute<rtcp>();
    register_attribute<ptime>();
    register_attribute<maxptime>();
    register_attribute<rtpmap>();
    register_attribute<fmtp>();
    register_attribute<transmit_mode>();
}

void _register_named_attribute_internal(const std::string& name, std::unique_ptr<_base_attribute_holder_creator> ptr) {
    _get_named_holders()[name] = std::move(ptr);
}

void _register_unnamed_attribute_internal(const std::string& name, std::unique_ptr<_base_unnamed_attribute_holder_creator> ptr) {
    _get_unnamed_holders()[name] = std::move(ptr);
}

std::optional<std::unique_ptr<_base_attribute_holder>> _create_named_attribute(const std::string& name) {
    const auto it = _get_named_holders().find(name);
    if (it != _get_named_holders().end()) {
        return it->second->create();
    } else {
        return std::nullopt;
    }
}

std::optional<std::pair<std::string, std::unique_ptr<_base_attribute_holder>>> _create_unnamed_attribute(const std::string& data) {
    for (const auto& [name, creator] : _get_unnamed_holders()) {
        if (creator->does_match(data)) {
            return {{name, creator->create()}};
        }
    }

    return std::nullopt;
}

}

const std::string message::unnamed_attr_generic_name = "";

std::istream& message::operator>>(std::istream& is) {
    while (is.peek() != std::istream::traits_type::eof()) {
        // read field
        const auto name = serialization::read_until(is, '=');
        serialization::consume(is, '=');
        serialization::consume_whitespaces(is);

        if (name == "a") {
            // this is an attribute
            auto attr = serialization::read_until_or(is, ':', '\r');
            if (serialization::try_consume(is, ':')) {
                // named attribute (attr=name)
                serialization::consume_whitespaces(is);

                auto holder_opt = attributes::_create_named_attribute(attr);
                if (holder_opt) {
                    auto holder = std::move(holder_opt.value());
                    holder->read(is);
                    add_named_attribute(attr, std::move(holder));
                } else {
                    auto holder = std::make_unique<attributes::_attribute_holder<attributes::generic_named_attribute>>();
                    holder->value.name = attr;
                    holder->read(is);
                    add_named_attribute(attr, std::move(holder));
                }
            } else {
                // unnamed attribute (attr=value)
                serialization::trim_whitespaces(attr);
                std::stringstream attr_is(attr);

                auto holder_opt = attributes::_create_unnamed_attribute(attr);
                if (holder_opt) {
                    auto [name, holder] = std::move(holder_opt.value());
                    holder->read(attr_is);
                    add_unnamed_attribute(name, std::move(holder));
                } else {
                    auto holder = std::make_unique<attributes::_attribute_holder<attributes::generic_unnamed_attribute>>();
                    holder->read(attr_is);
                    add_unnamed_attribute(unnamed_attr_generic_name, std::move(holder));
                }
            }
        } else {
            // this is a field
            auto holder_opt = fields::_create_field(name);
            if (holder_opt) {
                auto holder = std::move(holder_opt.value());
                holder->read(is);
                add_field(name, std::move(holder));
            } else {
                auto holder = std::make_unique<fields::_field_holder<fields::generic_field>>();
                holder->value.name = name;
                holder->read(is);
                add_field(name, std::move(holder));
            }
        }

        serialization::consume_whitespaces(is);
        if (serialization::try_consume(is, '\r')) {
            serialization::consume(is, '\n');
        } else if (is.peek() == std::istream::traits_type::eof()) {
            break;
        } else {
            throw serialization::unexpected_character();
        }
    }
    return is;
}

std::ostream& message::operator<<(std::ostream& os) {
    // dump fields
    for (auto& [name, holders] : m_fields) {
        for (const auto& holder : holders) {
            os << name << "=";
            holder->write(os);
            os << "\r\n";
        }
    }

    // dump named attributes
    for (auto& [name, holders] : m_named_attributes) {
        for (const auto& holder : holders) {
            os << "a=" << name << ":";
            holder->write(os);
            os << "\r\n";
        }
    }

    // dump unnamed attributes
    for (auto& holders: m_unnamed_attributes | std::views::values) {
        for (const auto& holder : holders) {
            os << "a= ";
            holder->write(os);
            os << "\r\n";
        }
    }

    return os;
}

void message::add_field(const std::string& name, std::unique_ptr<fields::_base_field_holder> holder) {
    const auto it = m_fields.find(name);
    if (it == m_fields.end()) {
        std::vector<std::unique_ptr<fields::_base_field_holder>> vector;
        vector.push_back(std::move(holder));
        m_fields[name] = std::move(vector);
    } else {
        it->second.push_back(std::move(holder));
    }
}

void message::add_named_attribute(const std::string& name, std::unique_ptr<attributes::_base_attribute_holder> holder) {
    const auto it = m_named_attributes.find(name);
    if (it == m_named_attributes.end()) {
        std::vector<std::unique_ptr<attributes::_base_attribute_holder>> vector;
        vector.push_back(std::move(holder));
        m_named_attributes[name] = std::move(vector);
    } else {
        it->second.push_back(std::move(holder));
    }
}

void message::add_unnamed_attribute(const std::string& name, std::unique_ptr<attributes::_base_attribute_holder> holder) {
    const auto it = m_unnamed_attributes.find(name);
    if (it == m_unnamed_attributes.end()) {
        std::vector<std::unique_ptr<attributes::_base_attribute_holder>> vector;
        vector.push_back(std::move(holder));
        m_unnamed_attributes[name] = std::move(vector);
    } else {
        it->second.push_back(std::move(holder));
    }
}

}
