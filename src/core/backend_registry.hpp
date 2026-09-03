#pragma once

#include <string>
#include <vector>
#include <memory>

namespace core {

struct Backend {
    std::string host;
    std::vector<std::string> urls;
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

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace core
