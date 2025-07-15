#pragma once

#include <iostream>

namespace looper::sdp {

enum class version {
    version_0
};

std::istream& operator>>(std::istream& is, version& version);
std::ostream& operator<<(std::ostream& os, version version);

enum class network_type {
    in
};

std::istream& operator>>(std::istream& is, network_type& network_type);
std::ostream& operator<<(std::ostream& os, network_type network_type);

enum class address_type {
    ipv4
};

std::istream& operator>>(std::istream& is, address_type& address_type);
std::ostream& operator<<(std::ostream& os, address_type address_type);

enum class media_type {
    audio,
    video
};

std::istream& operator>>(std::istream& is, media_type& media_type);
std::ostream& operator<<(std::ostream& os, media_type media_type);

enum class media_protocol {
    rtp_avp
};

std::istream& operator>>(std::istream& is, media_protocol& media_protocol);
std::ostream& operator<<(std::ostream& os, media_protocol media_protocol);

enum class transmit_mode {
    recvonly,
    sendrecv,
    sendonly,
    inactive
};

std::istream& operator>>(std::istream& is, transmit_mode& transmit_mode);
std::ostream& operator<<(std::ostream& os, transmit_mode transmit_mode);

}
