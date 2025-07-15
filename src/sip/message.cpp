
#include <looper_sip.h>
#include "util/streams.h"

namespace looper::sip {

namespace headers {

static std::unordered_map<std::string, std::unique_ptr<_base_header_holder_creator>>& _get_holders() {
    static std::unordered_map<std::string, std::unique_ptr<_base_header_holder_creator>> _header_creators;
    return _header_creators;
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

void __attribute__((constructor)) register_known_types() {
    headers::register_header<headers::cseq>();
    headers::register_header<headers::from>();
    headers::register_header<headers::to>();
    headers::register_header<headers::call_id>();
    headers::register_header<headers::content_length>();
    headers::register_header<headers::content_type>();
    headers::register_header<headers::max_forwards>();
    headers::register_header<headers::expires>();

    bodies::register_body<bodies::sdp_body>();
}

class unknown_request_or_status_line final : public std::exception {
};

class not_a_request final : public std::exception {
};

class not_a_response final : public std::exception {
};

void read_headers(std::istream& is, message& msg) {
    // read request/status line
    {
        std::regex pattern(R"(^(?:(?:(\w+)\s(.+)\sSIP\/(2\.0))|(?:SIP\/(2\.0)\s(\d+)\s(.+)))$)");
        const auto line = serialization::read_line(is);
        const auto match = serialization::parse(line, pattern);

        std::stringstream top_line_ss(line);
        if (match[1].matched && match[2].matched && match[3].matched) {
            // request
            sip::request_line request_line;
            sip::read_header_request_line(top_line_ss, request_line);
            msg.set_request_line(request_line);
        } else if (match[4].matched && match[5].matched && match[6].matched) {
            // status
            sip::status_line status_line;
            sip::read_header_status_line(top_line_ss, status_line);
            msg.set_status_line(status_line);
        } else {
            throw unknown_request_or_status_line();
        }

        serialization::consume_whitespaces(is);
        serialization::consume(is, '\r');
        serialization::consume(is, '\n');
    }

    while (is.peek() != std::istream::traits_type::eof()) {
        // read header
        const auto name = serialization::read_until(is, ':');
        serialization::consume(is, ':');
        serialization::consume_whitespaces(is);

        auto holder_opt = headers::_create_header(name);
        if (holder_opt) {
            auto holder = std::move(holder_opt.value());
            holder->read(is);
            msg.add_header(name, std::move(holder));
        } else {
            auto holder = std::make_unique<headers::_header_holder<headers::generic_header>>();
            holder->value.name = name;
            holder->read(is);
            msg.add_header(name, std::move(holder));
        }

        serialization::consume_whitespaces(is);
        if (serialization::try_consume(is, '\r')) {
            serialization::consume(is, '\n');
        } else if (is.peek() == std::istream::traits_type::eof()) {
            break;
        } else {
            throw serialization::unexpected_character();
        }

        if (serialization::try_consume(is, '\r')) {
            serialization::consume(is, '\n');
            break;
        }
    }
}

void read_body(std::istream& is, message& msg) {
    if (is.peek() != std::istream::traits_type::eof()) {
        // read body
        if (msg.has_header<headers::content_type>()) {
            const auto content_type_header = msg.header<headers::content_type>();
            const auto content_type = content_type_header.value;

            auto body_opt = bodies::_create_body(content_type);
            if (body_opt) {
                auto body = std::move(body_opt.value());
                body->operator>>(is);
                msg.set_body(std::move(body));
            } else {
                std::string body_data;
                is >> body_data;

                auto body = std::make_unique<bodies::generic_body>();
                body->m_content_type = content_type;
                body->m_data = body_data;
                msg.set_body(std::move(body));
            }
        } else {
            std::string body_data;
            is >> body_data;

            auto body = std::make_unique<bodies::generic_body>();
            body->m_data = body_data;
            msg.set_body(std::move(body));
        }
    }
}

ssize_t read_message(const std::span<const uint8_t> buffer, message& msg) {
    util::istream_buff buf(buffer);
    std::istream is(&buf);
    read_headers(is, msg);

    size_t expected_body_size;
    if (msg.has_header<headers::content_length>()) {
        expected_body_size = msg.header<headers::content_length>().value;
    } else {
        expected_body_size = 0;
    }

    if (static_cast<size_t>(is.tellg()) + expected_body_size > buffer.size()) {
        return -1;
    }

    if (expected_body_size > 0) {
        read_body(is, msg);
    }

    return is.tellg();
}

void write_headers(std::ostream& os, const message& msg) {
    // dump request/status line
    {
        if (msg.m_request_line) {
            sip::write_header_request_line(os, msg.m_request_line.value());
        } else if (msg.m_status_line) {
            sip::write_header_status_line(os, msg.m_status_line.value());
        } else {
            throw unknown_request_or_status_line();
        }

        os << "\r\n";
    }

    bool wrote_header = false;
    // dump headers
    for (auto& [name, holders] : msg.m_headers) {
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
}

void write_body(std::ostream& os, const message& msg) {
    if (msg.m_body) {
        msg.m_body->operator<<(os);
    }
}

ssize_t write_message(const std::span<uint8_t> buffer, const message& msg) {
    util::ostream_buff buf(buffer);
    std::ostream os(&buf);

    write_headers(os, msg);
    write_body(os, msg);

    return os.tellp();
}

std::istream& operator>>(std::istream& is, message& msg) {
    read_headers(is, msg);
    read_body(is, msg);
    return is;
}

std::ostream& operator<<(std::ostream& os, const message& msg) {
    write_headers(os, msg);
    write_body(os, msg);
    return os;
}

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
    auto copy = line;
    set_request_line(std::move(copy));
}

void message::set_request_line(sip::request_line&& line) {
    m_is_request = true;
    m_request_line = std::move(line);
    m_status_line = std::nullopt;
}

sip::status_line message::status_line() const {
    if (!m_status_line) {
        throw not_a_response();
    }

    return m_status_line.value();
}

void message::set_status_line(const sip::status_line& line) {
    auto copy = line;
    set_status_line(std::move(copy));
}

void message::set_status_line(sip::status_line&& line) {
    m_is_request = false;
    m_status_line = std::move(line);
    m_request_line = std::nullopt;
}

bool message::has_header(const std::string& name) const {
    return m_headers.contains(name);
}

bool message::has_body() const {
    return static_cast<bool>(m_body);
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
