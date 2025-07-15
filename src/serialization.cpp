
#include <string>

#include <looper_sip.h>

namespace looper::serialization {

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

void trim_whitespaces(std::string& str) {
    str.erase(remove(str.begin(), str.end(), ' '), str.end());
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
        } else if (peek == end_ch || peek == std::istream::traits_type::eof()) {
            break;
        } else {
            throw unexpected_character();
        }
    }

    return map;
}

arg_list read_args(std::istream& is, const char sep, const char end_ch) {
    arg_list list;

    while (true) {
        auto value = read_until_or(is, sep, end_ch);
        list.push_back(std::move(value));

        const auto peek = is.peek();
        if (peek == sep) {
            char cch;
            if (!is.get(cch)) {
                break;
            }
        } else if (peek == end_ch || peek == std::istream::traits_type::eof()) {
            break;
        } else {
            throw unexpected_character();
        }
    }

    return list;
}

std::string pop_tag(tag_map& map, const std::string& name) {
    auto opt = try_pop_tag(map, name);
    if (!opt) {
        throw std::runtime_error("No such tag");
    }

    return std::move(opt.value());
}

std::optional<std::string> try_pop_tag(tag_map& map, const std::string& name) {
    const auto it = map.find(name);
    if (it == map.end()) {
        return std::nullopt;
    }

    auto value = std::move(it->second);
    map.erase(it);

    if (value[0] == '\"') {
        return value.substr(1, value.size() - 2);
    }
    return value;
}

std::optional<uint16_t> try_pop_tag_int16(tag_map& map, const std::string& name) {
    const auto opt = try_pop_tag(map, name);
    if (!opt) {
        return std::nullopt;
    }

    return std::stoi(opt.value());;
}

void put_tag_with_quotes(tag_map& map, const std::string& name, const std::string& value) {
    map[name] = std::format("\"{}\"", value);
}

void write_tags(std::ostream& os, const tag_map& map, const char sep) {
    for (auto it = map.begin(); it != map.end();) {
        os << it->first;
        os << '=';
        os << it->second;

        ++it;
        if (it != map.end()) {
            os << sep;
        }
    }
}

void write_args(std::ostream& os, const arg_list& list, const char sep) {
    for (auto it = list.begin(); it != list.end();) {
        os << *it;

        ++it;
        if (it != list.end()) {
            os << sep;
        }
    }
}

}
