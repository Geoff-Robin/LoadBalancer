#pragma once

#include "core/backend_registry.hpp"

#include <optional>
#include <vector>

namespace core {

// Implement this interface to add a routing policy without changing proxying,
// backend registration, or health-check code.
class LoadBalancingStrategy {
  public:
    virtual ~LoadBalancingStrategy() = default;
    [[nodiscard]] virtual std::optional<Backend>
    select(const std::vector<Backend>& eligible_backends) = 0;
};

} // namespace core
