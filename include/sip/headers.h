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
    std::string value;
};

DEFINE_SIP_HEADER_READ(call_id) {
    h.value = serialization::read_line(is);
}

DEFINE_SIP_HEADER_WRITE(call_id) {
    os << h.value;
}

DECLARE_SIP_HEADER(content_length, "Content-Length") {
    uint32_t value;
};

DEFINE_SIP_HEADER_READ(content_length) {
    is >> h.value;
}

DEFINE_SIP_HEADER_WRITE(content_length) {
    os << h.value;
}

DECLARE_SIP_HEADER(content_type, "Content-Type") {
    std::string value;
};

DEFINE_SIP_HEADER_READ(content_type) {
    h.value = serialization::read_line(is);
}

DEFINE_SIP_HEADER_WRITE(content_type) {
    os << h.value;
}

DECLARE_SIP_HEADER(max_forwards, "Max-Forwards") {
    uint32_t value;
};

DEFINE_SIP_HEADER_READ(max_forwards) {
    is >> h.value;
}

DEFINE_SIP_HEADER_WRITE(max_forwards) {
    os << h.value;
}

DECLARE_SIP_HEADER(expires, "Expires") {
    uint32_t value;
};

DEFINE_SIP_HEADER_READ(expires) {
    is >> h.value;
}

DEFINE_SIP_HEADER_WRITE(expires) {
    os << h.value;
}
