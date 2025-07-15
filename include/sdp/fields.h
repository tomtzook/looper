#pragma once

#include <looper_tcp.h>
#include <looper_meta.h>
#include <sdp/base.h>

#include <util/serialize.h>

namespace looper::sdp::fields {

class field_not_found final : std::exception {};

struct _field {};

template<typename T>
concept _field_type = std::is_base_of_v<_field, T>;

struct _base_field_holder {
    virtual ~_base_field_holder() = default;
    virtual void read(std::istream& is) = 0;
    virtual void write(std::ostream& os) = 0;
};

template<_field_type T>
struct _field_holder final : _base_field_holder {
    void read(std::istream& is) override {
        looper::meta::_header_reader<T>::read(is, value);
    }
    void write(std::ostream& os) override {
        looper::meta::_header_writer<T>::write(os, value);
    }

    T value;
};

struct _base_field_holder_creator {
    virtual ~_base_field_holder_creator() = default;
    virtual std::unique_ptr<_base_field_holder> create() = 0;
};

template<_field_type T>
struct _field_holder_creator final : _base_field_holder_creator {
    std::unique_ptr<_base_field_holder> create() override {
        return std::make_unique<_field_holder<T>>();
    }
};

}

#define DECLARE_SDP_FIELD(h_name, str_name) DECLARE_HEADER(h_name, str_name, looper::sdp::fields, _field)
#define DEFINE_SDP_FIELD_READ(h_name) DEFINE_HEADER_READ(h_name, looper::sdp::fields)
#define DEFINE_SDP_FIELD_WRITE(h_name) DEFINE_HEADER_WRITE(h_name, looper::sdp::fields)

DECLARE_SDP_FIELD(sdp_version, "v") {
    sdp::version version;
};

DEFINE_SDP_FIELD_READ(sdp_version) {
    is >> h.version;
}

DEFINE_SDP_FIELD_WRITE(sdp_version) {
    os << h.version;
}

DECLARE_SDP_FIELD(session_name, "s") {
    std::string name;
};

DEFINE_SDP_FIELD_READ(session_name) {
    h.name = serialization::read_line(is);
}

DEFINE_SDP_FIELD_WRITE(session_name) {
    os << h.name;
}

DECLARE_SDP_FIELD(connection_info, "c") {
    sdp::network_type network_type;
    sdp::address_type address_type;
    std::string address;
};

DEFINE_SDP_FIELD_READ(connection_info) {
    is >> h.network_type;
    serialization::consume_whitespaces(is);
    is >> h.address_type;
    serialization::consume_whitespaces(is);
    h.address = serialization::read_line(is);
}

DEFINE_SDP_FIELD_WRITE(connection_info) {
    os << h.network_type;
    os << ' ';
    os << h.address_type;
    os << ' ';
    os << h.address;
}

DECLARE_SDP_FIELD(originator, "o") {
    std::string username;
    std::string id;
    std::string version;
    sdp::network_type network_type;
    sdp::address_type address_type;
    std::string address;
};

DEFINE_SDP_FIELD_READ(originator) {
    h.username = serialization::read_until(is, ' ');
    serialization::consume_whitespaces(is);
    h.id = serialization::read_until(is, ' ');
    serialization::consume_whitespaces(is);
    h.version = serialization::read_until(is, ' ');
    serialization::consume_whitespaces(is);
    is >> h.network_type;
    serialization::consume_whitespaces(is);
    is >> h.address_type;
    serialization::consume_whitespaces(is);
    h.address = serialization::read_line(is);
}

DEFINE_SDP_FIELD_WRITE(originator) {
    os << h.username;
    os << ' ';
    os << h.id;
    os << ' ';
    os << h.version;
    os << ' ';
    os << h.network_type;
    os << ' ';
    os << h.address_type;
    os << ' ';
    os << h.address;
}

DECLARE_SDP_FIELD(media_description, "m") {
    sdp::media_type type;
    uint16_t port;
    sdp::media_protocol protocol;
    std::vector<uint16_t> formats;
};

DEFINE_SDP_FIELD_READ(media_description) {
    is >> h.type;
    serialization::consume_whitespaces(is);
    is >> h.port;
    serialization::consume_whitespaces(is);
    is >> h.protocol;

    while (serialization::try_consume(is, ' ')) {
        serialization::consume_whitespaces(is);
        uint16_t format;
        is >> format;
        h.formats.push_back(format);
    }
}

DEFINE_SDP_FIELD_WRITE(media_description) {
    os << h.type;
    os << ' ';
    os << h.port;
    os << ' ';
    os << h.protocol;

    if (!h.formats.empty()) {
        for (auto& format : h.formats) {
            os << ' ' << format;
        }
    }
}

DECLARE_SDP_FIELD(time_description, "t") {
    uint32_t start_time;
    uint32_t stop_time;
};

DEFINE_SDP_FIELD_READ(time_description) {
    is >> h.start_time;
    serialization::consume_whitespaces(is);
    is >> h.stop_time;
}

DEFINE_SDP_FIELD_WRITE(time_description) {
    os << h.start_time;
    os << ' ';
    os << h.stop_time;
}

DECLARE_SDP_FIELD(bandwidth_information, "b") {
    std::string modifier;
    uint32_t value;
};

DEFINE_SDP_FIELD_READ(bandwidth_information) {
    h.modifier = serialization::read_until(is, ':');
    serialization::consume(is, ':');
    is >> h.value;
}

DEFINE_SDP_FIELD_WRITE(bandwidth_information) {
    os << h.modifier;
    os << ':';
    os << h.value;
}
