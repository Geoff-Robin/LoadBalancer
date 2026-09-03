#include "core/round_robin_strategy.hpp"

namespace core {

std::optional<Backend>
RoundRobinStrategy::select(const std::vector<Backend>& eligible_backends) {
    if (eligible_backends.empty()) {
        return std::nullopt;
    }
    const auto index = next_backend_.fetch_add(1, std::memory_order_relaxed);
    return eligible_backends[index % eligible_backends.size()];
}

} // namespace core
