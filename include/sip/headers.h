#pragma once

#include <optional>
#include <regex>

#include <looper_meta.h>

#include "base.h"

namespace looper::sip::headers {

class header_not_found final : std::exception {};

struct _header {};

template<typename T>
concept _header_type = std::is_base_of_v<_header, T>;

struct _base_header_holder {
    virtual ~_base_header_holder() = default;
    virtual void read(std::istream& is) = 0;
    virtual void write(std::ostream& os) = 0;
};

template<_header_type T>
struct _header_holder final : _base_header_holder {
    void read(std::istream& is) override {
        looper::meta::_header_reader<T>::read(is, value);
    }
    void write(std::ostream& os) override {
        looper::meta::_header_writer<T>::write(os, value);
    }

    T value;
};

struct _base_header_holder_creator {
    virtual ~_base_header_holder_creator() = default;
    virtual std::unique_ptr<_base_header_holder> create() = 0;
};

template<_header_type T>
struct _header_holder_creator final : _base_header_holder_creator {
    std::unique_ptr<_base_header_holder> create() override {
        return std::make_unique<_header_holder<T>>();
    }
};

}

#define DECLARE_SIP_HEADER(h_name, str_name) DECLARE_HEADER(h_name, str_name, looper::sip::headers, _header)
#define DEFINE_SIP_HEADER_READ(h_name) DEFINE_HEADER_READ(h_name, looper::sip::headers)
#define DEFINE_SIP_HEADER_WRITE(h_name) DEFINE_HEADER_WRITE(h_name, looper::sip::headers)


DECLARE_SIP_HEADER(generic_header, "") {
    std::string name;
    std::string value;
};

DEFINE_SIP_HEADER_READ(generic_header) {
    h.value = serialization::read_line(is);
}

DEFINE_SIP_HEADER_WRITE(generic_header) {
    os << h.value;
}

DECLARE_SIP_HEADER(cseq, "CSeq") {
    cseq() = default;
    cseq(const uint32_t seq_num, const sip::method method)
        : seq_num(seq_num), method(method) {}

    uint32_t seq_num;
    sip::method method;
};

DEFINE_SIP_HEADER_READ(cseq) {
    is >> h.seq_num;
    serialization::consume(is, ' ');
    is >> h.method;
}

DEFINE_SIP_HEADER_WRITE(cseq) {
    os << h.seq_num;
    os << ' ';
    os << h.method;
}

DECLARE_SIP_HEADER(from, "From") {
    from() = default;
    explicit from(
        const std::string_view uri,
        std::optional<std::string_view> name = std::nullopt,
        std::optional<std::string_view> tag = std::nullopt)
        : uri(uri)
        , name(name)
        , tag(tag)
    {}

    std::string uri;
    std::optional<std::string> name;
    std::optional<std::string> tag;
};

DEFINE_SIP_HEADER_READ(from) {
    const std::regex pattern("^(?:(.+)\\s)?<(.+)>(?:;tag=(.+))?$");

    const auto line = serialization::read_line(is);
    const auto match = serialization::parse(line, pattern);

    if (match[1].matched) {
        h.name = match[1].str();
    } else {
        h.name = std::nullopt;
    }

    h.uri = match[2].str();

    if (match[3].matched) {
        h.tag = match[3].str();
    } else {
        h.tag = std::nullopt;
    }
}

DEFINE_SIP_HEADER_WRITE(from) {
    if (h.name) {
        os << h.name.value();
        os << ' ';
    }

    os << '<';
    os << h.uri;
    os << '>';

    if (h.tag) {
        os << ';';
        os << "tag=";
        os << h.tag.value();
    }
}

DECLARE_SIP_HEADER(to, "To") {
    to() = default;
    explicit to(
        const std::string_view uri,
        std::optional<std::string_view> name = std::nullopt,
        std::optional<std::string_view> tag = std::nullopt)
        : uri(uri)
        , name(name)
        , tag(tag)
    {}

    std::string uri;
    std::optional<std::string> name;
    std::optional<std::string> tag;
};

DEFINE_SIP_HEADER_READ(to) {
    const std::regex pattern("^(?:(.+)\\s)?<(.+)>(?:;tag=(.+))?$");

    const auto line = serialization::read_line(is);
    const auto match = serialization::parse(line, pattern);

    if (match[1].matched) {
        h.name = match[1].str();
    } else {
        h.name = std::nullopt;
    }

    h.uri = match[2].str();

    if (match[3].matched) {
        h.tag = match[1].str();
    } else {
        h.tag = std::nullopt;
    }
}

DEFINE_SIP_HEADER_WRITE(to) {
    if (h.name) {
        os << h.name.value();
        os << ' ';
    }

    os << '<';
    os << h.uri;
    os << '>';

    if (h.tag) {
        os << ';';
        os << "tag=";
        os << h.tag.value();
    }
}

DECLARE_SIP_HEADER(call_id, "Call-ID") {
    call_id() = default;
    explicit call_id(const std::string_view value) : value(value) {}

    std::string value;
};

DEFINE_SIP_HEADER_READ(call_id) {
    h.value = serialization::read_line(is);
}

DEFINE_SIP_HEADER_WRITE(call_id) {
    os << h.value;
}

DECLARE_SIP_HEADER(content_length, "Content-Length") {
    content_length() = default;
    explicit content_length(const uint32_t value) : value(value) {}

    uint32_t value;
};

DEFINE_SIP_HEADER_READ(content_length) {
    is >> h.value;
}

DEFINE_SIP_HEADER_WRITE(content_length) {
    os << h.value;
}

DECLARE_SIP_HEADER(content_type, "Content-Type") {
    content_type() = default;
    explicit content_type(const std::string_view value) : value(value) {}

    std::string value;
};

DEFINE_SIP_HEADER_READ(content_type) {
    h.value = serialization::read_line(is);
}

DEFINE_SIP_HEADER_WRITE(content_type) {
    os << h.value;
}

DECLARE_SIP_HEADER(max_forwards, "Max-Forwards") {
    max_forwards() = default;
    explicit max_forwards(const uint32_t value) : value(value) {}

    uint32_t value;
};

DEFINE_SIP_HEADER_READ(max_forwards) {
    is >> h.value;
}

DEFINE_SIP_HEADER_WRITE(max_forwards) {
    os << h.value;
}

DECLARE_SIP_HEADER(expires, "Expires") {
    expires() = default;
    explicit expires(const uint32_t value) : value(value) {}

    uint32_t value;
};

DEFINE_SIP_HEADER_READ(expires) {
    is >> h.value;
}

DEFINE_SIP_HEADER_WRITE(expires) {
    os << h.value;
}

DECLARE_SIP_HEADER(contact, "Contact") {
    inet_address address;
    serialization::tag_map internal_tags;
    serialization::tag_map external_tags;
};

DEFINE_SIP_HEADER_READ(contact) {
    serialization::consume(is, "<sip:");
    h.address.ip = serialization::read_until(is, ':');
    is >> h.address.port;

    if (serialization::try_consume(is, ';')) {
        h.internal_tags = serialization::read_tags(is, ';', '>');
    }

    serialization::consume(is, '>');

    if (serialization::try_consume(is, ';')) {
        h.external_tags = serialization::read_tags(is, ';', '\r');
    }
}

DEFINE_SIP_HEADER_WRITE(contact) {
    os << "<sip:";
    os << h.address.ip;
    os << ':';
    os << h.address.port;

    if (!h.internal_tags.empty()) {
        os << ';';
        serialization::write_tags(os, h.internal_tags, ';');
    }

    os << '>';

    if (!h.external_tags.empty()) {
        os << ';';
        serialization::write_tags(os, h.external_tags, ';');
    }
}

DECLARE_SIP_HEADER(via, "Via") {
    via() = default;
    via(const sip::version version, const sip::transport transport, inet_address address)
        : version(version), transport(transport), address(std::move(address)) {}

    sip::version version;
    sip::transport transport;
    inet_address address;
    serialization::tag_map tags;
};

DEFINE_SIP_HEADER_READ(via) {
    is >> h.version;
    serialization::consume(is, '/');
    is >> h.transport;
    serialization::consume_whitespaces(is);

    h.address.ip = serialization::read_until(is, ':');
    is >> h.address.port;

    if (serialization::try_consume(is, ';')) {
        h.tags = serialization::read_tags(is, ';', '\r');
    }
}

DEFINE_SIP_HEADER_WRITE(via) {
    os << h.version;
    os << '/';
    os << h.transport;
    os << ' ';
    os << h.address.ip;
    os << ':';
    os << h.address.port;

    if (!h.tags.empty()) {
        os << ';';
        serialization::write_tags(os, h.tags, ';');
    }
}

DECLARE_SIP_HEADER(record_route, "Record-Route") {
    std::string user_info;
    std::string ip;
    std::optional<uint16_t> port;
    serialization::tag_map tags;
};

DEFINE_SIP_HEADER_READ(record_route) {
    serialization::consume(is, "<sip:");

    h.user_info = serialization::read_until(is, '@');
    serialization::consume(is, '@');

    h.ip = serialization::read_until_or(is, ':', ';', '>');

    h.port.reset();
    if (serialization::try_consume(is, ':')) {
        uint16_t port;
        is >> port;
        h.port = port;
    } else if (serialization::try_consume(is, ';')) {
        h.tags = serialization::read_tags(is, ';', '>');
    }

    serialization::consume(is, '>');
}

DEFINE_SIP_HEADER_WRITE(record_route) {
    os << "<sip:";
    os << h.user_info;
    os << '@';

    os << h.ip;
    if (h.port) {
        os << ':';
        os << h.port.value();
    }

    if (!h.tags.empty()) {
        os << ';';
        serialization::write_tags(os, h.tags, ';');
    }

    os << '>';
}

DECLARE_SIP_HEADER(authorization, "Authorization") {
    auth_scheme scheme;
    std::string username;
    std::string uri;
    std::string realm;
    auth_algorithm algorithm;
    std::string qop;
    std::string nonce;
    std::optional<uint16_t> nc;
    std::optional<std::string> cnonce;
    std::optional<std::string> response;
    serialization::tag_map additional_tags;
};

DEFINE_SIP_HEADER_READ(authorization) {
    is >> h.scheme;
    serialization::consume(is, ' ');

    auto tags = serialization::read_tags(is, ',', '\r');
    h.username = serialization::pop_tag(tags, "username");
    h.uri = serialization::pop_tag(tags, "uri");
    h.realm = serialization::pop_tag(tags, "realm");
    h.qop = serialization::pop_tag(tags, "qop");
    h.nonce = serialization::pop_tag(tags, "nonce");
    h.algorithm = serialization::pop_tag_as<auth_algorithm>(tags, "algorithm");
    h.nc = serialization::try_pop_tag_int16(tags, "nc");
    h.cnonce = serialization::try_pop_tag(tags, "cnonce");
    h.response = serialization::try_pop_tag(tags, "response");
    h.additional_tags = tags;
}

DEFINE_SIP_HEADER_WRITE(authorization) {
    os << h.scheme;
    os << ' ';

    serialization::tag_map tags;

    serialization::put_tag_with_quotes(tags, "username", h.username);
    serialization::put_tag_with_quotes(tags, "uri", h.uri);
    serialization::put_tag_with_quotes(tags, "realm", h.realm);
    serialization::put_tag_with_quotes(tags, "nonce", h.nonce);
    serialization::put_tag_as(tags, "algorithm", h.algorithm);

    tags["qop"] = h.qop;

    if (h.cnonce) {
        serialization::put_tag_with_quotes(tags, "cnonce", h.cnonce.value());
    }
    if (h.response) {
        serialization::put_tag_with_quotes(tags, "response", h.response.value());
    }

    tags.insert(h.additional_tags.begin(), h.additional_tags.end());

    serialization::write_tags(os, tags, ',');

    if (h.nc) {
        os << ",nc=" << std::setfill('0') << std::setw(8) << h.nc.value();
    }
}

DECLARE_SIP_HEADER(www_authorization, "WWW-Authenticate") {
    auth_scheme scheme;
    std::string uri;
    std::string realm;
    auth_algorithm algorithm;
    std::string qop;
    std::string nonce;
    serialization::tag_map additional_tags;
};

DEFINE_SIP_HEADER_READ(www_authorization) {
    is >> h.scheme;
    serialization::consume(is, ' ');

    auto tags = serialization::read_tags(is, ',', '\r');
    h.uri = serialization::pop_tag(tags, "uri");
    h.realm = serialization::pop_tag(tags, "realm");
    h.qop = serialization::pop_tag(tags, "qop");
    h.nonce = serialization::pop_tag(tags, "nonce");
    h.algorithm = serialization::pop_tag_as<auth_algorithm>(tags, "algorithm");
    h.additional_tags = tags;
}

DEFINE_SIP_HEADER_WRITE(www_authorization) {
    os << h.scheme;
    os << ' ';

    serialization::tag_map tags;

    serialization::put_tag_with_quotes(tags, "uri", h.uri);
    serialization::put_tag_with_quotes(tags, "realm", h.realm);
    serialization::put_tag_with_quotes(tags, "nonce", h.nonce);
    serialization::put_tag_as(tags, "algorithm", h.algorithm);

    tags["qop"] = h.qop;

    tags.insert(h.additional_tags.begin(), h.additional_tags.end());

    serialization::write_tags(os, tags, ',');
}