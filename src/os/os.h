#pragma once

#include <memory>

namespace looper::os {

using descriptor = int;

enum class resource_type {
    none,
    event
};

class resource {
public:
    resource(descriptor descriptor, resource_type type);
    virtual ~resource() = default;

    [[nodiscard]] descriptor get_descriptor() const;
    [[nodiscard]] resource_type get_type() const;

private:
    descriptor m_descriptor;
    resource_type m_type;
};

class event : public resource {
public:
    explicit event(descriptor descriptor);
    virtual ~event() override = default;

    virtual void set() = 0;
    virtual void clear() = 0;
};

}
