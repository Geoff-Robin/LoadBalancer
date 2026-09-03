#pragma once

#include "core/load_balancing_strategy.hpp"

#include <atomic>

namespace core {

class RoundRobinStrategy final : public LoadBalancingStrategy {
  public:
    [[nodiscard]] std::optional<Backend>
    select(const std::vector<Backend>& eligible_backends) override;

  private:
    std::atomic_size_t next_backend_{0};
};

} // namespace core
