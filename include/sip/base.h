#pragma once

#include <iostream>
#include <regex>

#include <looper_types.h>

namespace looper::sip {

enum class method {
    invite = 0,
    ack,
    bye,
    cancel,
    update,
    info,
    subscribe,
    notify,
    refer,
    message,
    options,
    register_,
};

std::istream& operator>>(std::istream& is, method& method);
std::ostream& operator<<(std::ostream& os, method method);

enum class status_code {
    trying = 100,
    ringing = 180,
    call_being_forwarded = 181,
    queued = 182,
    session_progress = 183,
    early_dialog_terminated = 199,
    ok = 200,
    accepted = 202,
    no_notification = 204,
    multiple_choices = 300,
    moved_permanently = 301,
    moved_temporarily = 302,
    use_proxy = 305,
    alternative_service = 380,
    bad_request = 400,
    unauthorized = 401,
    payment_required = 402,
    forbidden = 403,
    not_found = 404,
    method_not_allowed = 405,
    not_acceptable = 406,
    proxy_authentication_required = 407,
    request_timeout = 408,
    conflict = 409,
    gone = 410,
    length_required = 411,
    conditional_request_failed = 412,
    request_entity_too_large = 413,
    request_uri_too_long = 414,
    unsupported_media_type = 415,
    unsupported_uri_scheme = 416,
    unknown_resource_priority = 417,
    bad_extension = 420,
    extension_required = 421,
    session_interval_too_small = 422,
    interval_too_brief = 423,
    bad_location_information = 424,
    bad_alert_information = 425,
    use_identity_header = 428,
    provide_referrer_identity = 429,
    flow_failed = 430,
    anonymity_disallowed = 433,
    bad_identity_info = 436,
    unsupported_certificate = 437,
    invalid_identity_header = 438,
    first_hop_lacks_outbound_support = 439,
    max_breadth_exceeded = 440,
    bad_info_package = 469,
    consent_needed = 470,
    temporarily_unavailable = 480,
    call_transaction_does_not_exist = 481,
    loop_detected = 482,
    too_many_hops = 483,
    address_incomplete = 484,
    ambiguous = 485,
    busy_here = 486,
    request_terminated = 487,
    not_acceptable_here = 488,
    bad_event = 489,
    request_pending = 491,
    undecipherable = 493,
    security_agreement_required = 494,
    internal_server_error = 500,
    not_implemented = 501,
    bad_gateway = 502,
    service_unavailable = 503,
    server_timeout = 504,
    version_not_supported = 505,
    message_too_large = 513,
    push_notification_not_supported = 555,
    precondition_failure = 580,
    busy_everywhere = 600,
    decline = 603,
    does_not_exist_anywhere = 604,
    not_acceptable_global = 606,
    unwanted = 607,
    rejected = 608
};

std::istream& operator>>(std::istream& is, status_code& code);
std::ostream& operator<<(std::ostream& os, status_code code);
const char* status_code_message(status_code code);

enum class version {
    version_2_0
};

std::istream& operator>>(std::istream& is, version& version);
std::ostream& operator<<(std::ostream& os, version version);

enum class transport {
    tcp,
    udp
};

std::istream& operator>>(std::istream& is, transport& transport);
std::ostream& operator<<(std::ostream& os, transport transport);

namespace serialization {

template<typename t_, typename t2_>
bool is_any_of(t_ t, t2_ t2) {
    return t == t2;
}

template<typename t_, typename t2_, typename... args_>
bool is_any_of(t_ t, t2_ t2, args_... args) {
    return (t == t2) || is_any_of(t, args...);
}

template<typename... args_>
std::string read_until_or(std::istream& is, args_... args) {
    std::string buffer;

    int pch = is.peek();
    while (pch != is.eof() && !is_any_of(pch, args...)) {
        char c_ch;
        if (is.get(c_ch)) {
            buffer += c_ch;
        } else {
            break;
        }

        pch = is.peek();
    }

    return buffer;
}

std::string read_until(std::istream& is, char ch);
std::string read_line(std::istream& is);
std::smatch parse(const std::string& data, const std::regex& pattern);
void consume_whitespaces(std::istream& is);
bool try_consume(std::istream& is, char ch);
void consume(std::istream& is, char ch);
void consume(std::istream& is, const char* str);

using tag_map = std::map<std::string, std::string>;
tag_map read_list(std::istream& is, char sep, char end_ch);

void write_list(std::ostream& os, const tag_map& map, char sep);

}

}
