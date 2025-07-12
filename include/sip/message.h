#pragma once

#include "headers.h"
#include "bodies.h"


DECLARE_HEADER_NO_PARENT(request_line, "", looper::sip) {
    request_line() = default;
    request_line(const sip::method method, std::string uri, const sip::version version)
        : method(method), uri(std::move(uri)), version(version)
    {}

    sip::method method;
    std::string uri;
    sip::version version;
};

DEFINE_HEADER_READ(request_line, looper::sip) {
    is >> h.method;
    serialization::consume_whitespaces(is);
    h.uri = serialization::read_until(is, ' ');
    serialization::consume_whitespaces(is);
    is >> h.version;
}

DEFINE_HEADER_WRITE(request_line, looper::sip) {
    os << h.method;
    os << ' ';
    os << h.uri;
    os << ' ';
    os << h.version;
}

DECLARE_HEADER_NO_PARENT(status_line, "", looper::sip) {
    sip::version version;
    sip::status_code code;
    std::string description;
};

DEFINE_HEADER_READ(status_line, looper::sip) {
    is >> h.version;
    serialization::consume_whitespaces(is);
    is >> h.code;
    serialization::consume_whitespaces(is);
    h.description = serialization::read_line(is);
}

DEFINE_HEADER_WRITE(status_line, looper::sip) {
    os << h.version;
    os << ' ';
    os << h.code;
    os << ' ';
    os << h.description;
}

namespace looper::sip {

namespace headers {

void _register_header_internal(const std::string& name, std::unique_ptr<_base_header_holder_creator> ptr);

template<_header_type T>
void register_header() {
    const auto name = looper::meta::_header_name<T>::name();
    auto holder = std::make_unique<_header_holder_creator<T>>();
    _register_header_internal(name, std::move(holder));
}

}

namespace bodies {

void _register_body_internal(const std::string& content_type, std::unique_ptr<_base_body_creator> ptr);

template<_body_type T>
void register_body() {
    auto holder = std::make_unique<_body_creator<T>>();
    const auto content_type = holder->create()->content_type();

    _register_body_internal(content_type, std::move(holder));
}

}

class message;

void read_headers(std::istream& is, message& msg);
void read_body(std::istream& is, message& msg);
ssize_t read_message(std::span<const uint8_t> buffer, message& msg);

void write_headers(std::ostream& os, const message& msg);
void write_body(std::ostream& os, const message& msg);
ssize_t write_message(std::span<uint8_t> buffer, const message& msg);

std::istream& operator>>(std::istream& is, message& msg);
std::ostream& operator<<(std::ostream& os, const message& msg);

class message {
public:
    message() = default;
    message(const message&) = delete;
    message(message&&) = default;
    ~message() = default;

    [[nodiscard]] bool is_request() const;

    [[nodiscard]] sip::request_line request_line() const;
    void set_request_line(const sip::request_line& line);
    void set_request_line(sip::request_line&& line);

    [[nodiscard]] sip::status_line status_line() const;
    void set_status_line(const sip::status_line& line);
    void set_status_line(sip::status_line&& line);

    template<headers::_header_type T>
    [[nodiscard]] bool has_header() const;
    template<headers::_header_type T>
    T header() const;
    template<headers::_header_type T>
    std::vector<T> headers() const;
    template<headers::_header_type T>
    void add_header(const T& header);
    template<headers::_header_type T>
    void add_header(T&& header);

    [[nodiscard]] bool has_body() const;
    template<bodies::_body_type T>
    const T& body() const;
    template<bodies::_body_type T>
    void set_body(T&& body);

private:
    void add_header(const std::string& name, std::unique_ptr<headers::_base_header_holder> holder);
    void set_body(std::unique_ptr<bodies::body> body);

    bool m_is_request = false;
    std::optional<sip::request_line> m_request_line;
    std::optional<sip::status_line> m_status_line;
    std::map<std::string, std::vector<std::unique_ptr<headers::_base_header_holder>>> m_headers;
    std::unique_ptr<bodies::body> m_body;

    friend void read_headers(std::istream& is, message& msg);
    friend void read_body(std::istream& is, message& msg);
    friend void write_headers(std::ostream& os, const message& msg);
    friend void write_body(std::ostream& os, const message& msg);
};

template<headers::_header_type T>
bool message::has_header() const {
    const auto name = looper::meta::_header_name<T>::name();
    auto it = m_headers.find(name);
    if (it == m_headers.end()) {
        return false;
    } else {
        return true;
    }
}

template<headers::_header_type T>
T message::header() const {
    const auto name = looper::meta::_header_name<T>::name();
    auto it = m_headers.find(name);
    if (it == m_headers.end()) {
        throw headers::header_not_found();
    }

    if (it->second.size() < 1) {
        throw headers::header_not_found();
    }

    auto holder = reinterpret_cast<headers::_header_holder<T>*>(it->second[0].get());
    return holder->value;
}

template<headers::_header_type T>
std::vector<T> message::headers() const {
    const auto name = looper::meta::_header_name<T>::name();
    auto it = m_headers.find(name);
    if (it == m_headers.end()) {
        throw headers::header_not_found();
    }

    if (it->second.size() < 1) {
        throw headers::header_not_found();
    }

    std::vector<T> result;
    result.reserve(it->second.size());

    for (auto& ptr : it->second) {
        auto holder = reinterpret_cast<headers::_header_holder<T>*>(it->second[0].get());
        result.push_back(holder->value);
    }

    return result;
}

template<headers::_header_type T>
void message::add_header(const T& header) {
    T header_copy = header;
    add_header(std::move(header_copy));
}

template<headers::_header_type T>
void message::add_header(T&& header) {
    const auto name = looper::meta::_header_name<T>::name();

    auto holder = std::make_unique<headers::_header_holder<T>>();
    holder->value = std::forward<T>(header);

    add_header(name, std::move(holder));
}

template<bodies::_body_type T>
const T& message::body() const {
    if (!m_body) {
        throw bodies::has_no_body();
    }

    auto* body = m_body.get();
    return *reinterpret_cast<T*>(body);
}

template<bodies::_body_type T>
void message::set_body(T&& body) {
    const auto content_type = body.content_type();
    auto ptr = std::make_unique<T>(std::forward<T>(body));
    set_body(std::move(ptr));

    headers::content_type content_type_header;
    content_type_header.value = content_type;
    add_header(content_type_header);
}

}
