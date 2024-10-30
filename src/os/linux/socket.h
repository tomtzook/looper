#pragma once

#include <cstddef>
#include <memory>

namespace looper::os {

enum class sockopt_type : size_t {
    reuse_port,
    keep_alive
};

template<sockopt_type opt_, typename type_>
struct _sockopt {
    static constexpr auto opt = opt_;
    using type = type_;
};

template<typename t_>
struct _sockopt_hack : public std::false_type {};

#define define_sockopt(name, opt, type) \
    using sockopt_ ##name = _sockopt<opt, type>; \
    template<> struct _sockopt_hack<sockopt_ ##name> : public std::true_type {};

define_sockopt(reuseport, sockopt_type::reuse_port, bool);
define_sockopt(keepalive, sockopt_type::keep_alive, bool);

class linux_base_socket {
public:
    explicit linux_base_socket(int fd);
    virtual ~linux_base_socket();

    [[nodiscard]] int get_fd() const;

    int get_call_error();
    int get_internal_error();

    void setoption(sockopt_type opt, void* value, size_t size);

    template<typename t_,
            typename std::enable_if<
                    _sockopt_hack<t_>::value,
                    bool>::type = 0
    >
    void setoption(typename t_::type value) {
        if constexpr (std::is_same_v<typename t_::type, bool>) {
            int _value = static_cast<int>(value);
            setoption(t_::opt, &_value, sizeof(_value));
        } else {
            setoption(t_::opt, &value, sizeof(typename t_::type));
        }
    }

    void configure_blocking(bool blocking);

    void bind(const std::string_view& ip, uint16_t port);
    void bind(uint16_t port);

    void listen(size_t backlog_size);
    int accept();

    bool connect(std::string_view ip, uint16_t port);
    void finalize_connect();

    size_t read(uint8_t* buffer, size_t buffer_size);
    size_t write(const uint8_t* buffer, size_t size);

    void close();

private:
    [[nodiscard]] inline bool is_blocking() const {
        return m_is_blocking;
    }

    [[nodiscard]] inline bool is_disabled() const {
        return m_disabled;
    }

    inline void enable() {
        m_disabled = false;
    }

    inline void disable() {
        m_disabled = true;
    }

    void handle_call_error(int code=0);
    void check_internal_error(int code=0);

    void throw_if_disabled() const;
    void throw_if_closed() const;

    int m_fd;
    bool m_is_blocking;
    bool m_disabled;
};

}
