#pragma once

#include <exception>
#include <iostream>

#include <sdp/message.h>

namespace looper::sip::bodies {

class has_no_body final : std::exception {};

class body {
public:
    virtual ~body() = default;

    [[nodiscard]] virtual std::string content_type() const = 0;

    virtual std::istream& operator>>(std::istream& is) = 0;
    virtual std::ostream& operator<<(std::ostream& os) = 0;
};

template<typename T>
concept _body_type = std::is_base_of_v<body, T>;

struct _base_body_creator {
    virtual ~_base_body_creator() = default;
    virtual std::unique_ptr<body> create() = 0;
};

template<_body_type T>
struct _body_creator final : _base_body_creator {
    std::unique_ptr<body> create() override {
        return std::make_unique<T>();
    }
};

class generic_body final : public body {
public:
    generic_body();

    [[nodiscard]] std::string content_type() const override;

    std::istream& operator>>(std::istream& is) override;
    std::ostream& operator<<(std::ostream& os) override;

    std::optional<std::string> m_content_type;
    std::string m_data;
};

class sdp_body final : public body {
public:
    sdp_body() = default;
    sdp_body(const sdp_body&) = delete;
    sdp_body(sdp_body&&) = default;
    explicit sdp_body(sdp::message&& msg) : m_message(std::move(msg)) {}

    [[nodiscard]] std::string content_type() const override;

    std::istream& operator>>(std::istream& is) override;
    std::ostream& operator<<(std::ostream& os) override;

    sdp::message m_message;
};

}
