
#include "looper_types.h"


namespace looper {

inet_address_view::inet_address_view(std::string_view ip, uint16_t port)
    : ip(ip)
    , port(port)
{}

inet_address_view::inet_address_view(const inet_address& other)
    : ip(other.ip)
    , port(other.port)
{}

inet_address_view& inet_address_view::operator=(const inet_address& other) {
    ip = other.ip;
    port = other.port;
    return *this;
}

inet_address::inet_address(std::string_view ip, uint16_t port)
    : ip(ip)
    , port(port)
{}

inet_address::inet_address(const inet_address_view& other)
    : ip(other.ip)
    , port(other.port)
{}

inet_address& inet_address::operator=(const inet_address_view& other) {
    ip = other.ip;
    port = other.port;
    return *this;
}

}
