#pragma once

#include <iostream>
#include <regex>

#include <looper_types.h>

namespace looper::serialization {

class no_regex_match final : public std::exception {
};

class unexpected_character final : public std::exception {
};

using arg_list = std::vector<std::string>;
using tag_map = std::map<std::string, std::string>;

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
    while (pch != std::istream::traits_type::eof() && !is_any_of(pch, args...)) {
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
void trim_whitespaces(std::string& str);

tag_map read_tags(std::istream& is, char sep, char end_ch);
arg_list read_args(std::istream& is, char sep, char end_ch);

std::string pop_tag(tag_map& map, const std::string& name);
std::optional<std::string> try_pop_tag(tag_map& map, const std::string& name);
std::optional<uint16_t> try_pop_tag_int16(tag_map& map, const std::string& name);

template<typename t_>
t_ pop_tag_as(tag_map& map, const std::string& name) {
    const auto tag = pop_tag(map, name);
    std::stringstream ss(tag);
    t_ t;
    ss >> t;

    return t;
}

void put_tag_with_quotes(tag_map& map, const std::string& name, const std::string& value);

template<typename t_>
void put_tag_as(tag_map& map, const std::string& name, const t_& value) {
    std::stringstream ss;
    ss << value;
    map[name] = ss.str();
}

void write_tags(std::ostream& os, const tag_map& map, char sep);
void write_args(std::ostream& os, const arg_list& list, char sep);

}
