#include <cstring>

#include <sdp/base.h>
#include <util/serialize.h>

namespace looper::sdp {

std::istream& operator>>(std::istream& is, version& version) {
    uint16_t verint;
    is >> verint;

    version = static_cast<enum version>(verint);

    return is;
}

std::ostream& operator<<(std::ostream& os, version version) {
    os << static_cast<uint16_t>(version);
    return os;
}

std::istream& operator>>(std::istream& is, network_type& network_type) {
    char buff[2];
    is.read(buff, 2);

    if (strcasecmp(buff, "in") == 0) {
        network_type = network_type::in;
    } else {
        throw std::runtime_error("Invalid network type");
    }

    return is;
}

std::ostream& operator<<(std::ostream& os, const network_type network_type) {
    switch (network_type) {
        case network_type::in:
            os << "IN";
            break;
        default:
            throw std::runtime_error("Invalid network type");
    }
    return os;
}

std::istream& operator>>(std::istream& is, address_type& address_type) {
    char buff[3];
    is.read(buff, 3);

    if (strcasecmp(buff, "IP4") == 0) {
        address_type = address_type::ipv4;
    } else {
        throw std::runtime_error("Invalid address type");
    }

    return is;
}

std::ostream& operator<<(std::ostream& os, const address_type address_type) {
    switch (address_type) {
        case address_type::ipv4:
            os << "IP4";
            break;
        default:
            throw std::runtime_error("Invalid address type");
    }

    return os;
}

std::istream& operator>>(std::istream& is, media_type& media_type) {
    char buff[5];
    is.read(buff, 5);

    if (strcasecmp(buff, "audio") == 0) {
        media_type = media_type::audio;
    } else if (strcasecmp(buff, "video") == 0) {
        media_type = media_type::video;
    } else {
        throw std::runtime_error("Invalid media type");
    }

    return is;
}

std::ostream& operator<<(std::ostream& os, const media_type media_type) {
    switch (media_type) {
        case media_type::audio:
            os << "audio";
            break;
        case media_type::video:
            os << "video";
            break;
        default:
            throw std::runtime_error("Invalid media type");
    }

    return os;
}

std::istream& operator>>(std::istream& is, media_protocol& media_protocol) {
    char buff[7];
    is.read(buff, 7);

    if (strcasecmp(buff, "RTP/AVP") == 0) {
        media_protocol = media_protocol::rtp_avp;
    } else {
        throw std::runtime_error("Invalid media protocol");
    }

    return is;
}

std::ostream& operator<<(std::ostream& os, const media_protocol media_protocol) {
    switch (media_protocol) {
        case media_protocol::rtp_avp:
            os << "RTP/AVP";
            break;
        default:
            throw std::runtime_error("Invalid media protocol");
    }
    return os;
}

std::istream& operator>>(std::istream& is, transmit_mode& transmit_mode) {
    char buff[8];
    is.read(buff, 8);

    if (strcasecmp(buff, "recvonly") == 0) {
        transmit_mode = transmit_mode::recvonly;
    } else if (strcasecmp(buff, "sendrecv") == 0) {
        transmit_mode = transmit_mode::sendrecv;
    } else if (strcasecmp(buff, "sendonly") == 0) {
        transmit_mode = transmit_mode::sendonly;
    } else if (strcasecmp(buff, "inactive") == 0) {
        transmit_mode = transmit_mode::inactive;
    } else {
        throw std::runtime_error("Invalid transmit_mode");
    }

    return is;
}

std::ostream& operator<<(std::ostream& os, const transmit_mode transmit_mode) {
    switch (transmit_mode) {
        case transmit_mode::recvonly:
            os << "recvonly";
            break;
        case transmit_mode::sendrecv:
            os << "sendrecv";
            break;
        case transmit_mode::sendonly:
            os << "sendonly";
            break;
        case transmit_mode::inactive:
            os << "inactive";
            break;
    }
    return os;
}

}
