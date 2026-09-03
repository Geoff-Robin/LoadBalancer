#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace core {

enum class BackendHealth { unknown, healthy, unhealthy };

struct Backend {
    std::string host;
    std::vector<std::string> urls;
    BackendHealth health = BackendHealth::unknown;
    std::optional<std::string> health_checked_at;
};

class BackendRegistry {
  public:
    explicit BackendRegistry(std::string database_path = "load_balancer.db");
    ~BackendRegistry();

    BackendRegistry(const BackendRegistry&) = delete;
    BackendRegistry& operator=(const BackendRegistry&) = delete;
    BackendRegistry(BackendRegistry&&) noexcept;
    BackendRegistry& operator=(BackendRegistry&&) noexcept;

    // Inserts a backend or replaces the URLs for an existing host. Returns true
    // when a new backend was added and false when an existing backend was updated.
    bool register_backend(Backend backend);
    [[nodiscard]] std::vector<Backend> backends() const;
    void set_backend_health(const std::string& host, BackendHealth health);

  private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace core
