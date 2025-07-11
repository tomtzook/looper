#pragma once


namespace looper::meta {

template<typename T>
struct _header_name {};
template<typename T>
struct _header_reader {};
template<typename T>
struct _header_writer {};

}

#define DECLARE_HEADER_NO_PARENT(h_name, str_name, h_namespace) \
    namespace h_namespace { \
    struct h_name; \
    } \
    namespace looper::meta { template<> struct _header_name<h_namespace::h_name> { static constexpr auto name() { return (str_name) ; } }; } \
    struct h_namespace::h_name

#define DECLARE_HEADER(h_name, str_name, h_namespace, h_super) \
    namespace h_namespace { \
    struct h_name; \
    } \
    namespace looper::meta { template<> struct _header_name<h_namespace::h_name> { static constexpr auto name() { return (str_name) ; } }; } \
    struct h_namespace::h_name : h_namespace::h_super

#define DEFINE_HEADER_READ(h_name, h_namespace) \
    namespace h_namespace { \
    static void read_header_ ##h_name(std::istream& is, h_name & h); \
    } \
    namespace looper::meta { template<> struct _header_reader<h_namespace::h_name> { static void read(std::istream& is, h_namespace::h_name & h) { return h_namespace::read_header_ ##h_name(is, h); } }; } \
    static void h_namespace::read_header_ ##h_name(std::istream& is, h_name & h)

#define DEFINE_HEADER_WRITE(h_name, h_namespace) \
    namespace h_namespace { \
    static void write_header_ ##h_name(std::ostream& os, const h_name & h); \
    } \
    namespace looper::meta { template<> struct _header_writer<h_namespace::h_name> { static void write(std::ostream& os, const h_namespace::h_name & h) { return h_namespace::write_header_ ##h_name(os, h); } }; } \
    static void h_namespace::write_header_ ##h_name(std::ostream& os, const h_name & h)
