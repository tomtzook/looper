#pragma once

#include <iostream>
#include <regex>

#include <looper_types.h>

namespace looper::serialization {

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
tag_map read_tags(std::istream& is, char sep, char end_ch);
void write_tags(std::ostream& os, const tag_map& map, char sep);

}
