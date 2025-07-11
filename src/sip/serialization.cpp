
#include <string>

#include <looper_sip.h>

namespace looper::sip::serialization {

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

std::istream& consume(std::istream& is, const char ch) {
    char cch;

    if (!is.get(cch) || cch != ch) {
        throw unexpected_character();
    }

    return is;
}

}
