#pragma once

#include <string>

namespace core {

struct BackendEndpoint {
    std::string host;
    std::string service;
};

// Parses "host", "host:port", and "[ipv6]:port" backend addresses.
[[nodiscard]] BackendEndpoint parse_backend_endpoint(const std::string& backend);

} // namespace core
