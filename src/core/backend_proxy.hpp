#pragma once

#include "core/backend_health_monitor.hpp"
#include "core/load_balancing_strategy.hpp"

#include <boost/beast/http.hpp>

#include <memory>
#include <optional>

namespace core {

// Filters backends by route and health, delegates selection to a strategy,
// then relays the request to the selected backend.
class BackendProxy {
  public:
    BackendProxy(std::shared_ptr<BackendRegistry> registry,
                 std::shared_ptr<BackendHealthMonitor> health_monitor,
                 std::unique_ptr<LoadBalancingStrategy> strategy);

    [[nodiscard]] boost::beast::http::response<boost::beast::http::string_body>
    forward(const boost::beast::http::request<boost::beast::http::string_body>& request);

  private:
    [[nodiscard]] std::optional<Backend>
    select_backend(const boost::beast::http::request<boost::beast::http::string_body>& request);

    std::shared_ptr<BackendRegistry> registry_;
    std::shared_ptr<BackendHealthMonitor> health_monitor_;
    std::unique_ptr<LoadBalancingStrategy> strategy_;
};

} // namespace core
