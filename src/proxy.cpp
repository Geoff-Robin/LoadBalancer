#include "core/backend_registry.hpp"
#include "routes/backend_routes.hpp"
#include "routes/health_routes.hpp"
#include "slam/server.hpp"
#include <exception>
#include <memory>
#include <spdlog/spdlog.h>

int main() {
    try {
        auto backends = std::make_shared<core::BackendRegistry>();
        slam::Server server{3000};
        server.add_router(routes::make_health_routes());
        server.add_router(routes::make_backend_routes(std::move(backends)));
        server.run();
    } catch (const std::exception& e) {
        spdlog::critical("Server stopped: {}", e.what());
        return 1;
    }
    return 0;
}
