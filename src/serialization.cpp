
#include <string>

#include <looper_sip.h>

namespace looper::serialization {

class no_regex_match final : public std::exception {
};

class unexpected_character final : public std::exception {
};

std::string read_until(std::istream& is, const char ch) {
    std::string buffer;
    while (is.peek() != ch) {
        char c_ch;
        if (is.get(c_ch)) {
            buffer += c_ch;
        } else {
            break;
        }
    }

    return buffer;
}

std::string read_line(std::istream& is) {
    const auto str = read_until(is, '\r');

    return str;
}

std::smatch parse(const std::string& data, const std::regex& pattern) {
    std::smatch match;
    if (std::regex_match(data, match, pattern)) {
        return match;
    } else {
        throw no_regex_match();
    }
}

void consume_whitespaces(std::istream& is) {
    while (is.peek() == ' ') {
        char ch;
        if (!is.get(ch)) {
            break;
        }
    }
}

bool try_consume(std::istream& is, const char ch) {
    const auto peek = is.peek();
    if (peek != ch) {
        return false;
    }

    char cch;
    is.get(cch);

    return true;
}

void consume(std::istream& is, const char ch) {
    if (!try_consume(is, ch)) {
        throw unexpected_character();
    }
}

void consume(std::istream& is, const char* str) {
    while (*str) {
        consume(is, *str);
        str++;
    }
}

tag_map read_tags(std::istream& is, const char sep, const char end_ch) {
    tag_map map;

    while (true) {
        const auto name = read_until(is, '=');
        consume(is, '=');
        const auto value = read_until_or(is, sep, end_ch);
        map[name] = value;

        const auto peek = is.peek();
        if (peek == sep) {
            char cch;
            if (!is.get(cch)) {
                break;
            }
        } else if (peek == end_ch || peek == is.eof()) {
            break;
        } else {
            throw unexpected_character();
        }
    }

    return map;
}

void write_tags(std::ostream& os, const tag_map& map, const char sep) {
    for (auto& [key, value] : map) {
        os << key;
        os << sep;
        os << value;
    }
}

}
