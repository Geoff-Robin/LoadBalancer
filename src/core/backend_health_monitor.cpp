#include "core/backend_health_monitor.hpp"
#include "core/backend_endpoint.hpp"

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <spdlog/spdlog.h>

#include <chrono>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace core {
namespace {

namespace beast = boost::beast;
namespace http = beast::http;
} // namespace

BackendHealthMonitor::BackendHealthMonitor(std::shared_ptr<BackendRegistry> registry,
                                           std::chrono::minutes check_interval)
    : registry_(std::move(registry)), check_interval_(check_interval) {
    if (!registry_) {
        throw std::invalid_argument("backend health monitor requires a backend registry");
    }
    if (check_interval_ <= std::chrono::minutes::zero()) {
        throw std::invalid_argument("health-check interval must be greater than zero");
    }
    thread_ = std::thread(&BackendHealthMonitor::health_check_loop, this);
}

BackendHealthMonitor::~BackendHealthMonitor() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
    }
    wakeup_.notify_one();
    thread_.join();
}

bool BackendHealthMonitor::is_healthy(const Backend& backend) const {
    // Let new registrations receive traffic until their first scheduled check.
    return backend.health != BackendHealth::unhealthy;
}

void BackendHealthMonitor::mark_unhealthy(const std::string& host) {
    spdlog::warn("Backend {} marked unhealthy after a proxy failure", host);
    registry_->set_backend_health(host, BackendHealth::unhealthy);
}

void BackendHealthMonitor::health_check_loop() {
    for (;;) {
        try {
            check_all_backends();
        } catch (const std::exception& error) {
            spdlog::error("Backend health-check pass failed: {}", error.what());
        }

        std::unique_lock<std::mutex> lock(mutex_);
        if (wakeup_.wait_for(lock, check_interval_, [this] { return stopping_; })) {
            return;
        }
    }
}

void BackendHealthMonitor::check_all_backends() {
    const auto backends = registry_->backends();
    std::unordered_set<std::string> registered_hosts;
    for (const auto& backend : backends) {
        registered_hosts.insert(backend.host);
        const bool healthy = check_backend(backend);

        const auto updated_health = healthy ? BackendHealth::healthy : BackendHealth::unhealthy;
        if (backend.health != updated_health) {
            spdlog::info("Backend {} is {}", backend.host, healthy ? "healthy" : "unhealthy");
        }
        registry_->set_backend_health(backend.host, updated_health);
    }

    for (auto pool = health_pools_.begin(); pool != health_pools_.end();) {
        if (registered_hosts.find(pool->first) == registered_hosts.end()) {
            pool = health_pools_.erase(pool);
        } else {
            ++pool;
        }
    }
}

bool BackendHealthMonitor::check_backend(const Backend& backend) {
    try {
        const auto endpoint = parse_backend_endpoint(backend.host);
        auto pool = health_pools_.find(backend.host);
        if (pool == health_pools_.end()) {
            requests::BackendPoolConfig config;
            config.host = endpoint.host;
            config.service = endpoint.service;
            config.max_connections = 1;
            config.connect_timeout = std::chrono::seconds{5};
            pool = health_pools_
                       .emplace(backend.host, std::make_unique<requests::BackendConnectionPool>(
                                                  health_check_ioc_, std::move(config)))
                       .first;
        }

        auto lease = pool->second->try_acquire();
        if (!lease) {
            return false;
        }

        try {
            http::request<http::empty_body> request{http::verb::get, "/health", 11};
            request.set(http::field::host, backend.host);
            request.keep_alive(true);
            http::write(lease->stream(), request);

            beast::flat_buffer buffer;
            http::response<http::string_body> response;
            http::read(lease->stream(), buffer, response);
            if (!response.keep_alive()) {
                lease->invalidate();
            }
            return response.result_int() >= 200 && response.result_int() < 300;
        } catch (...) {
            lease->invalidate();
            throw;
        }
    } catch (const std::exception& error) {
        spdlog::debug("Health check failed for {}: {}", backend.host, error.what());
        return false;
    }
}

} // namespace core
