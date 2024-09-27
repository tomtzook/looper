#pragma once

namespace looper::os {

using descriptor = int;

class resource {
public:
    explicit resource(descriptor descriptor);
    virtual ~resource() = default;

    [[nodiscard]] descriptor get_descriptor() const;

private:
    descriptor m_descriptor;
};

}
