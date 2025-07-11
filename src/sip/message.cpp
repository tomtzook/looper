
#include <looper_sip.h>

namespace looper::sip {

namespace headers {

class unknown_header final : public std::exception {
};

static std::unordered_map<std::string, std::unique_ptr<_base_header_holder_creator>>& _get_holders() {
    static std::unordered_map<std::string, std::unique_ptr<_base_header_holder_creator>> _header_creators;
    return _header_creators;
}

void __attribute__((constructor)) register_known_headers() {
    register_header<cseq>();
    register_header<from>();
    register_header<to>();
    register_header<call_id>();
    register_header<content_length>();
    register_header<content_type>();
    register_header<max_forwards>();
    register_header<expires>();
}

void _register_header_internal(const std::string& name, std::unique_ptr<_base_header_holder_creator> ptr) {
    _get_holders()[name] = std::move(ptr);
}

std::optional<std::unique_ptr<_base_header_holder>> _create_header(const std::string& name) {
    const auto it = _get_holders().find(name);
    if (it != _get_holders().end()) {
        return it->second->create();
    } else {
        return std::nullopt;
    }
}

}

namespace bodies {

class unknown_body final : public std::exception {
};

static std::unordered_map<std::string, std::unique_ptr<_base_body_creator>>& _get_holders() {
    static std::unordered_map<std::string, std::unique_ptr<_base_body_creator>> creators;
    return creators;
}

void _register_body_internal(const std::string& content_type, std::unique_ptr<_base_body_creator> ptr) {
    _get_holders()[content_type] = std::move(ptr);
}

std::optional<std::unique_ptr<body>> _create_body(const std::string& content_type) {
    const auto it = _get_holders().find(content_type);
    if (it != _get_holders().end()) {
        return it->second->create();
    } else {
        return std::nullopt;
    }
}

}

class unknown_request_or_status_line final : public std::exception {
};

class not_a_request final : public std::exception {
};

class not_a_response final : public std::exception {
};

bool message::is_request() const {
    return m_is_request;
}

sip::request_line message::request_line() const {
    if (!m_request_line) {
        throw not_a_request();
    }

    return m_request_line.value();
}

void message::set_request_line(const sip::request_line& line) {
    m_is_request = true;
    m_request_line = line;
    m_status_line = std::nullopt;
}

sip::status_line message::status_line() const {
    if (!m_status_line) {
        throw not_a_response();
    }

    return m_status_line.value();
}

void message::set_status_line(const sip::status_line& line) {
    m_is_request = false;
    m_status_line = line;
    m_status_line = std::nullopt;
}

bool message::has_body() const {
    return static_cast<bool>(m_body);
}

std::istream& message::operator>>(std::istream& is) {
    // read request/status line
    {
        std::regex pattern("^(?:(?:(\\w+)\\s(.+)\\sSIP\\/(2\\.0))|(?:SIP\\/(2\\.0)\\s(\\d+)\\s(.+)))$");
        const auto line = serialization::read_line(is);
        const auto match = serialization::parse(line, pattern);

        std::stringstream top_line_ss(line);
        if (match[1].matched && match[2].matched && match[3].matched) {
            // request
            sip::request_line request_line;
            sip::read_header_request_line(top_line_ss, request_line);
            m_request_line = request_line;
        } else if (match[4].matched && match[5].matched && match[6].matched) {
            // status
            sip::status_line status_line;
            sip::read_header_status_line(top_line_ss, status_line);
            m_status_line = status_line;
        } else {
            throw unknown_request_or_status_line();
        }

        serialization::consume_whitespaces(is);
        serialization::consume(is, '\r');
        serialization::consume(is, '\n');
    }

    while (is.peek() != std::char_traits<char>::eof()) {
        // read header
        const auto name = serialization::read_until(is, ':');
        serialization::consume(is, ':');
        serialization::consume_whitespaces(is);

        auto holder_opt = headers::_create_header(name);
        if (holder_opt) {
            auto holder = std::move(holder_opt.value());
            holder->read(is);
            add_header(name, std::move(holder));
        } else {
            auto holder = std::make_unique<headers::_header_holder<headers::generic_header>>();
            holder->value.name = name;
            holder->read(is);
            add_header(name, std::move(holder));
        }

        serialization::consume_whitespaces(is);
        serialization::consume(is, '\r');
        serialization::consume(is, '\n');

        if (is.peek() == '\r') {
            serialization::consume(is, '\r');
            serialization::consume(is, '\n');
            break;
        }
    }

    if (is.peek() != std::char_traits<char>::eof()) {
        // read body
        if (has_header<headers::content_type>()) {
            const auto content_type_header = header<headers::content_type>();
            const auto content_type = content_type_header.value;

            auto body_opt = bodies::_create_body(content_type);
            if (body_opt) {
                auto body = std::move(body_opt.value());
                body->operator>>(is);
                set_body(std::move(body));
            } else {
                std::string body_data;
                is >> body_data;

                auto body = std::make_unique<bodies::generic_body>();
                body->m_content_type = content_type;
                body->m_data = body_data;
                set_body(std::move(body));
            }
        } else {
            std::string body_data;
            is >> body_data;

            auto body = std::make_unique<bodies::generic_body>();
            body->m_data = body_data;
            set_body(std::move(body));
        }
    }

    return is;
}

std::ostream& message::operator<<(std::ostream& os) {
    // dump request/status line
    {
        if (m_request_line) {
            sip::write_header_request_line(os, m_request_line.value());
        } else if (m_status_line) {
            sip::write_header_status_line(os, m_status_line.value());
        } else {
            throw unknown_request_or_status_line();
        }

        os << "\r\n";
    }

    bool wrote_header = false;
    // dump headers
    for (auto& [name, holders] : m_headers) {
        for (const auto& holder : holders) {
            os << name << ": ";
            holder->write(os);
            os << "\r\n";
            wrote_header = true;
        }
    }

    // dump body
    os << "\r\n";
    if (!wrote_header) {
        os << "\r\n";
    }

    if (m_body) {
        m_body->operator<<(os);
    }

    return os;
}

void message::add_header(const std::string& name, std::unique_ptr<headers::_base_header_holder> holder) {
    const auto it = m_headers.find(name);
    if (it == m_headers.end()) {
        std::vector<std::unique_ptr<headers::_base_header_holder>> vector;
        vector.push_back(std::move(holder));
        m_headers[name] = std::move(vector);
    } else {
        it->second.push_back(std::move(holder));
    }
}

void message::set_body(std::unique_ptr<bodies::body> body) {
    m_body = std::move(body);
}

}
