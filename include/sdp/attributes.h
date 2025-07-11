#pragma once

#include <looper_tcp.h>
#include <looper_meta.h>
#include <sdp/base.h>

namespace looper::sdp::attributes {

class attribute_not_found final : std::exception {};

struct _attribute {};

template<typename T>
concept _attribute_type = std::is_base_of_v<_attribute, T>;

struct _base_attribute_holder {
    virtual ~_base_attribute_holder() = default;
    virtual void read(std::istream& is) = 0;
    virtual void write(std::ostream& os) = 0;
};

template<_attribute_type T>
struct _attribute_holder final : _base_attribute_holder {
    void read(std::istream& is) override {
        looper::meta::_header_reader<T>::read(is, value);
    }
    void write(std::ostream& os) override {
        looper::meta::_header_writer<T>::write(os, value);
    }

    T value;
};

struct _base_attribute_holder_creator {
    virtual ~_base_attribute_holder_creator() = default;
    virtual std::unique_ptr<_base_attribute_holder> create() = 0;
};

template<_attribute_type T>
struct _attribute_holder_creator final : _base_attribute_holder_creator {
    std::unique_ptr<_base_attribute_holder> create() override {
        return std::make_unique<_attribute_holder<T>>();
    }
};

}

#define DECLARE_SDP_ATTRIBUTE(h_name, str_name) DECLARE_HEADER(h_name, str_name, looper::sdp::attributes, _attribute)
#define DEFINE_SDP_ATTRIBUTE_READ(h_name) DEFINE_HEADER_READ(h_name, looper::sdp::attributes)
#define DEFINE_SDP_ATTRIBUTE_WRITE(h_name) DEFINE_HEADER_WRITE(h_name, looper::sdp::attributes)

DECLARE_SDP_ATTRIBUTE(generic_attribute, "") {
    std::string name;
    std::string value;
};

DEFINE_SDP_ATTRIBUTE_READ(generic_attribute) {
    h.value = serialization::read_line(is);
}

DEFINE_SDP_ATTRIBUTE_WRITE(generic_attribute) {
    os << h.value;
}

DECLARE_SDP_ATTRIBUTE(rtcp, "rtcp") {
    uint16_t port;
};

DEFINE_SDP_ATTRIBUTE_READ(rtcp) {
    is >> h.port;
}

DEFINE_SDP_ATTRIBUTE_WRITE(rtcp) {
    os << h.port;
}

DECLARE_SDP_ATTRIBUTE(ptime, "ptime") {
    uint32_t time;
};

DEFINE_SDP_ATTRIBUTE_READ(ptime) {
    is >> h.time;
}

DEFINE_SDP_ATTRIBUTE_WRITE(ptime) {
    os << h.time;
}
