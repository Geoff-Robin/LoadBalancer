#include "core/backend_registry.hpp"
#include "core/backend_health_monitor.hpp"
#include "core/backend_proxy.hpp"
#include "core/round_robin_strategy.hpp"
#include "routes/backend_routes.hpp"
#include "routes/health_routes.hpp"
#include "slam/server.hpp"
#include <exception>
#include <memory>
#include <spdlog/spdlog.h>

int main() {
    try {
        auto backends = std::make_shared<core::BackendRegistry>();
        auto health_monitor = std::make_shared<core::BackendHealthMonitor>(backends);
        auto proxy = std::make_shared<core::BackendProxy>(
            backends, health_monitor, std::make_unique<core::RoundRobinStrategy>());
        slam::Server server{3000};
        server.add_router(routes::make_health_routes());
        server.add_router(routes::make_backend_routes(backends));
        server.set_fallback([proxy = std::move(proxy)](const slam::Request& request) {
            return proxy->forward(request);
        });
        server.run();
    } catch (const std::exception& e) {
        spdlog::critical("Server stopped: {}", e.what());
        return 1;
    }
    return 0;
}
