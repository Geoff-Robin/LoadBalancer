#pragma once

#include "core/backend_registry.hpp"
#include "requests/backend_connection_pool.hpp"

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

namespace core {

// Independently tracks whether registered backends respond successfully to
// GET /health. Health data is intentionally in-memory and transient.
class BackendHealthMonitor {
  public:
    explicit BackendHealthMonitor(
        std::shared_ptr<BackendRegistry> registry,
        std::chrono::minutes check_interval = std::chrono::minutes{5});
    ~BackendHealthMonitor();

    BackendHealthMonitor(const BackendHealthMonitor&) = delete;
    BackendHealthMonitor& operator=(const BackendHealthMonitor&) = delete;

    [[nodiscard]] bool is_healthy(const Backend& backend) const;
    void mark_unhealthy(const std::string& host);

  private:
    void health_check_loop();
    void check_all_backends();
    [[nodiscard]] bool check_backend(const Backend& backend);

    std::shared_ptr<BackendRegistry> registry_;
    std::chrono::minutes check_interval_;
    mutable std::mutex mutex_;
    requests::net::io_context health_check_ioc_;
    std::unordered_map<std::string, std::unique_ptr<requests::BackendConnectionPool>> health_pools_;
    std::condition_variable wakeup_;
    std::thread thread_;
    bool stopping_ = false;
};

} // namespace core
