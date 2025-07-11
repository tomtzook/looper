
#include <string>
#include <cstring>

#include <looper_sip.h>

namespace looper::sip {

class unknown_method final : public std::exception {
};

class unknown_status_code final : public std::exception {
};

class unknown_version final : public std::exception {
};

static constexpr bool is_digit(const char ch) {
    return ch >= '0' && ch <= '9';
}

static constexpr bool is_letter(const char ch) {
    return ch >= 'a' && ch <= 'z' || ch >= 'A' && ch <= 'Z';
}

std::istream& operator>>(std::istream& is, method& method) {
    std::string buffer;
    while (is_letter(is.peek())) {
        char ch;
        is.get(ch);
        buffer += ch;
    }

    if (buffer == "INVITE") {
        method = method::invite;
    } else if (buffer == "ACK") {
        method = method::ack;
    } else if (buffer == "BYE") {
        method = method::bye;
    } else if (buffer == "CANCEL") {
        method = method::cancel;
    } else if (buffer == "UPDATE") {
        method = method::update;
    } else if (buffer == "INFO") {
        method = method::info;
    } else if (buffer == "SUBSCRIBE") {
        method = method::subscribe;
    } else if (buffer == "NOTIFY") {
        method = method::notify;
    } else if (buffer == "REFER") {
        method = method::refer;
    } else if (buffer == "MESSAGE") {
        method = method::message;
    } else if (buffer == "OPTIONS") {
        method = method::options;
    } else if (buffer == "REGISTER") {
        method = method::register_;
    } else {
        throw unknown_method();
    }

    return is;
}

std::ostream& operator<<(std::ostream& os, const method method) {
    switch (method) {
        case method::invite:
            os << "INVITE";
            break;
        case method::ack:
            os << "ACK";
            break;
        case method::bye:
            os << "BYE";
            break;
        case method::cancel:
            os << "CANCEL";
            break;
        case method::update:
            os << "UPDATE";
            break;
        case method::info:
            os << "INFO";
            break;
        case method::subscribe:
            os << "SUBSCRIBE";
            break;
        case method::notify:
            os << "NOTIFY";
            break;
        case method::refer:
            os << "REFER";
            break;
        case method::message:
            os << "MESSAGE";
            break;
        case method::options:
            os << "OPTIONS";
            break;
        case method::register_:
            os << "REGISTER";
            break;
        default:
            throw unknown_method();
    }

    return os;
}

std::istream& operator>>(std::istream& is, status_code& code) {
    std::string buffer;
    while (is_digit(is.peek())) {
        char ch;
        is.get(ch);
        buffer += ch;
    }

    const auto int_code = std::stoi(buffer);
    code = static_cast<status_code>(int_code);

    return is;
}

std::ostream& operator<<(std::ostream& os, const status_code code) {
    os << std::to_string(static_cast<uint16_t>(code));
    return os;
}

const char* status_code_message(const status_code code) {
    switch (code) {
        case status_code::trying:
            return "TRYING";
        case status_code::ringing:
            return "RINGING";
        case status_code::call_being_forwarded:
            return "CALL BEING FORWARDED";
        case status_code::queued:
            return "QUEUED";
        case status_code::session_progress:
            return "SESSION PROGRESS";
        case status_code::early_dialog_terminated:
            return "EARLY DIALOG TERMINATED";
        case status_code::ok:
            return "OK";
        case status_code::accepted:
            return "ACCEPTED";
        case status_code::no_notification:
            return "NO NOTIFICATION";
        case status_code::multiple_choices:
            return "MULTIPLE CHOICES";
        case status_code::moved_permanently:
            return "MOVED PERMANENTLY";
        case status_code::moved_temporarily:
            return "MOVED TEMPORARILY";
        case status_code::use_proxy:
            return "USE PROXY";
        case status_code::alternative_service:
            return "ALTERNATIVE SERVICE";
        case status_code::bad_request:
            return "BAD REQUEST";
        case status_code::unauthorized:
            return "UNAUTHORIZED";
        case status_code::payment_required:
            return "PAYMENT REQUIRED";
        case status_code::forbidden:
            return "FORBIDDEN";
        case status_code::not_found:
            return "NOT FOUND";
        case status_code::method_not_allowed:
            return "METHOD NOT ALLOWED";
        case status_code::not_acceptable:
            return "NOT ACCEPTABLE";
        case status_code::proxy_authentication_required:
            return "PROXY AUTHENTICATION REQUIRED";
        case status_code::request_timeout:
            return "REQUEST TIMEOUT";
        case status_code::conflict:
            return "CONFLICT";
        case status_code::gone:
            return "GONE";
        case status_code::length_required:
            return "LENGTH REQUIRED";
        case status_code::conditional_request_failed:
            return "CONDITIONAL REQUEST FAILED";
        case status_code::request_entity_too_large:
            return "REQUEST ENTITY TOO LARGE";
        case status_code::request_uri_too_long:
            return "REQUEST URI TOO LONG";
        case status_code::unsupported_media_type:
            return "UNSUPPORTED MEDIA TYPE";
        case status_code::unsupported_uri_scheme:
            return "UNSUPPORTED URI SCHEME";
        case status_code::unknown_resource_priority:
            return "UNKNOWN RESOURCE PRIORITY";
        case status_code::bad_extension:
            return "BAD EXTENSION";
        case status_code::extension_required:
            return "EXTENSION REQUIRED";
        case status_code::session_interval_too_small:
            return "SESSION INTERVAL TOO SMALL";
        case status_code::interval_too_brief:
            return "INTERVAL TOO BRIEF";
        case status_code::bad_location_information:
            return "BAD LOCATION INFORMATION";
        case status_code::bad_alert_information:
            return "BAD ALERT INFORMATION";
        case status_code::use_identity_header:
            return "USE IDENTITY HEADER";
        case status_code::provide_referrer_identity:
            return "PROVIDE REFERRER IDENTITY";
        case status_code::flow_failed:
            return "FLOW FAILED";
        case status_code::anonymity_disallowed:
            return "ANONYMITY DISALLOWED";
        case status_code::bad_identity_info:
            return "BAD IDENTITY INFO";
        case status_code::unsupported_certificate:
            return "UNSUPPORTED CERTIFICATE";
        case status_code::invalid_identity_header:
            return "INVALID IDENTITY HEADER";
        case status_code::first_hop_lacks_outbound_support:
            return "FIRST HOP LACKS OUTBOUND SUPPORT";
        case status_code::max_breadth_exceeded:
            return "MAX BREADTH EXCEEDED";
        case status_code::bad_info_package:
            return "BAD INFO PACKAGE";
        case status_code::consent_needed:
            return "CONSENT NEEDED";
        case status_code::temporarily_unavailable:
            return "TEMPORARILY UNAVAILABLE";
        case status_code::call_transaction_does_not_exist:
            return "CALL TRANSACTION DOES NOT EXIST";
        case status_code::loop_detected:
            return "LOOP DETECTED";
        case status_code::too_many_hops:
            return "TOO MANY HOPS";
        case status_code::address_incomplete:
            return "ADDRESS INCOMPLETE";
        case status_code::ambiguous:
            return "AMBIGUOUS";
        case status_code::busy_here:
            return "BUSY HERE";
        case status_code::request_terminated:
            return "REQUEST TERMINATED";
        case status_code::not_acceptable_here:
            return "NOT ACCEPTABLE HERE";
        case status_code::bad_event:
            return "BAD EVENT";
        case status_code::request_pending:
            return "REQUEST PENDING";
        case status_code::undecipherable:
            return "UNDECIPHERABLE";
        case status_code::security_agreement_required:
            return "SECURITY AGREEMENT REQUIRED";
        case status_code::internal_server_error:
            return "INTERNAL SERVER ERROR";
        case status_code::not_implemented:
            return "NOT IMPLEMENTED";
        case status_code::bad_gateway:
            return "BAD GATEWAY";
        case status_code::service_unavailable:
            return "SERVICE UNAVAILABLE";
        case status_code::server_timeout:
            return "SERVER TIMEOUT";
        case status_code::version_not_supported:
            return "VERSION NOT SUPPORTED";
        case status_code::message_too_large:
            return "MESSAGE TOO LARGE";
        case status_code::push_notification_not_supported:
            return "PUSH NOTIFICATION NOT SUPPORTED";
        case status_code::precondition_failure:
            return "PRECONDITION FAILURE";
        case status_code::busy_everywhere:
            return "BUSY EVERYWHERE";
        case status_code::decline:
            return "DECLINE";
        case status_code::does_not_exist_anywhere:
            return "DOES NOT EXIST ANYWHERE";
        case status_code::not_acceptable_global:
            return "NOT ACCEPTABLE GLOBAL";
        case status_code::unwanted:
            return "UNWANTED";
        case status_code::rejected:
            return "REJECTED";
        default:
            throw unknown_status_code();
    }
}

std::istream& operator>>(std::istream& is, version& version) {
    serialization::consume(is, 'S');
    serialization::consume(is, 'I');
    serialization::consume(is, 'P');
    serialization::consume(is, '/');

    uint16_t version_int;
    is >> version_int;

    switch (version_int) {
        case 2:
            version = version::version_2_0;
            serialization::consume(is, '.');
            serialization::consume(is, '0');
            break;
        default:
            throw unknown_version();
    }

    return is;
}

std::ostream& operator<<(std::ostream& os, const version version) {
    os << "SIP/";

    switch (version) {
        case version::version_2_0:
            os << "2.0";
            break;
        default:
            throw unknown_version();
    }

    return os;
}

std::istream& operator>>(std::istream& is, transport& transport) {
    const auto name = serialization::read_until(is, ' ');
    if (strcasecmp(name.c_str(), "tcp") == 0) {
        transport = transport::tcp;
    } else if (strcasecmp(name.c_str(), "udp") == 0) {
        transport = transport::udp;
    } else {
        throw std::runtime_error("Invalid transport");
    }

    return is;
}

std::ostream& operator<<(std::ostream& os, const transport transport) {
    switch (transport) {
        case transport::tcp:
            os << "TCP";
            break;
        case transport::udp:
            os << "UDP";
            break;
        default:
            throw std::runtime_error("Invalid transport");
    }
    return os;
}

namespace bodies {

generic_body::generic_body()
    : m_content_type()
    , m_data()
{}

std::string generic_body::content_type() const {
    return m_content_type.value_or("");
}

std::istream& generic_body::operator>>(std::istream& is) {
    is >> m_data;
    return is;
}

std::ostream& generic_body::operator<<(std::ostream& os) {
    os << m_data;
    return os;
}

std::string sdp_body::content_type() const {
    return "application/sdp";
}

std::istream& sdp_body::operator>>(std::istream& is) {
    return m_message.operator>>(is);
}
std::ostream& sdp_body::operator<<(std::ostream& os) {
    return m_message.operator<<(os);
}

}

}
