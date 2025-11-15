#pragma once

namespace looper {

enum class event_type : uint32_t {
    none = 0,
    in = (0x1 << 0),
    out = (0x1 << 1),
    error = (0x1 << 2),
    hung = (0x1 << 3)
};

constexpr event_type operator~(const event_type lhs) {
    return static_cast<event_type>(~static_cast<uint32_t>(lhs));
}

constexpr event_type operator|(const event_type lhs, const event_type rhs) {
    return static_cast<event_type>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

constexpr event_type operator&(const event_type lhs, const event_type rhs) {
    return static_cast<event_type>(static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs));
}

constexpr event_type& operator|=(event_type& lhs, const event_type rhs) {
    lhs = lhs | rhs;
    return lhs;
}

constexpr event_type& operator&=(event_type& lhs, const event_type rhs) {
    lhs = lhs & rhs;
    return lhs;
}

constexpr bool operator==(const event_type lhs, const uint32_t rhs) {
    return static_cast<uint32_t>(lhs) == rhs;
}

constexpr bool operator!=(const event_type lhs, const uint32_t rhs) {
    return static_cast<uint32_t>(lhs) != rhs;
}

}
