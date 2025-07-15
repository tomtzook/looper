#pragma once

#include <looper_meta.h>
#include <sdp/base.h>

namespace looper::sdp::attributes {

class attribute_not_found final : std::exception {};

struct _attribute {};

template<typename T>
concept _attribute_type = std::is_base_of_v<_attribute, T>;

struct _named_attribute : _attribute {};

struct _unnamed_attribute : _attribute {};

template<typename T>
struct _attribute_identifier {};

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
struct _named_attribute_holder_creator final : _base_attribute_holder_creator {
    std::unique_ptr<_base_attribute_holder> create() override {
        return std::make_unique<_attribute_holder<T>>();
    }
};

struct _base_unnamed_attribute_holder_creator : _base_attribute_holder_creator {
    [[nodiscard]] virtual bool does_match(const std::string& data) const = 0;
};

template<_attribute_type T>
struct _unnamed_attribute_holder_creator final : _base_unnamed_attribute_holder_creator {
    std::unique_ptr<_base_attribute_holder> create() override {
        return std::make_unique<_attribute_holder<T>>();
    }

    [[nodiscard]] bool does_match(const std::string& data) const override {
        return std::regex_search(data, regex);
    }

    std::regex regex = std::regex(_attribute_identifier<T>::regex());
};

}

#define DECLARE_SDP_NAMED_ATTRIBUTE(h_name, str_name) DECLARE_HEADER(h_name, str_name, looper::sdp::attributes, _named_attribute)
#define DECLARE_SDP_UNNAMED_ATTRIBUTE(h_name, str_regex) \
    namespace looper::sdp::attributes { \
    struct h_name; \
    template<> struct _attribute_identifier<h_name> { static constexpr auto regex() { return (str_regex) ; } }; \
    } \
    DECLARE_HEADER(h_name, #h_name, looper::sdp::attributes, _unnamed_attribute)
#define DEFINE_SDP_ATTRIBUTE_READ(h_name) DEFINE_HEADER_READ(h_name, looper::sdp::attributes)
#define DEFINE_SDP_ATTRIBUTE_WRITE(h_name) DEFINE_HEADER_WRITE(h_name, looper::sdp::attributes)

DECLARE_SDP_NAMED_ATTRIBUTE(rtcp, "rtcp") {
    uint16_t port;
};

DEFINE_SDP_ATTRIBUTE_READ(rtcp) {
    is >> h.port;
}

DEFINE_SDP_ATTRIBUTE_WRITE(rtcp) {
    os << h.port;
}

DECLARE_SDP_NAMED_ATTRIBUTE(ptime, "ptime") {
    uint32_t time;
};

DEFINE_SDP_ATTRIBUTE_READ(ptime) {
    is >> h.time;
}

DEFINE_SDP_ATTRIBUTE_WRITE(ptime) {
    os << h.time;
}

DECLARE_SDP_NAMED_ATTRIBUTE(maxptime, "maxptime") {
    uint32_t time;
};

DEFINE_SDP_ATTRIBUTE_READ(maxptime) {
    is >> h.time;
}

DEFINE_SDP_ATTRIBUTE_WRITE(maxptime) {
    os << h.time;
}

DECLARE_SDP_NAMED_ATTRIBUTE(rtpmap, "rtpmap") {
    uint32_t format;
    std::string mime_type;
    uint32_t sample_rate;
    std::optional<uint32_t> channels;
};

DEFINE_SDP_ATTRIBUTE_READ(rtpmap) {
    is >> h.format;
    serialization::consume_whitespaces(is);
    h.mime_type = serialization::read_until(is, '/');
    is >> h.sample_rate;

    if (serialization::try_consume(is, '/')) {
        uint32_t channels;
        is >> channels;
        h.channels = channels;
    } else {
        h.channels = std::nullopt;
    }
}

DEFINE_SDP_ATTRIBUTE_WRITE(rtpmap) {
    os << h.format;
    os << ' ';
    os << h.mime_type;
    os << '/';
    os << h.sample_rate;

    if (h.channels) {
        os << '/';
        os << h.channels.value();
    }
}

DECLARE_SDP_NAMED_ATTRIBUTE(fmtp, "fmtp") {
    uint32_t format;
    serialization::arg_list params;
};

DEFINE_SDP_ATTRIBUTE_READ(fmtp) {
    is >> h.format;
    serialization::consume_whitespaces(is);
    h.params = serialization::read_args(is, ';', '\r');
}

DEFINE_SDP_ATTRIBUTE_WRITE(fmtp) {
    os << h.format;

    if (!h.params.empty()) {
        os << ' ';
        serialization::write_args(os, h.params, ';');
    }
}

DECLARE_SDP_UNNAMED_ATTRIBUTE(transmit_mode, "^(?:(?:recvonly)|(?:sendrecv)|(?:sendonly)|(?:inactive))$") {
    sdp::transmit_mode mode;
};

DEFINE_SDP_ATTRIBUTE_READ(transmit_mode) {
    is >> h.mode;
}

DEFINE_SDP_ATTRIBUTE_WRITE(transmit_mode) {
    os << h.mode;
}
