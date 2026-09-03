#include "core/backend_endpoint.hpp"

#include <stdexcept>

namespace core {

BackendEndpoint parse_backend_endpoint(const std::string& backend) {
    if (backend.empty()) {
        throw std::invalid_argument("backend host must not be empty");
    }
    if (backend.front() == '[') {
        const auto closing_bracket = backend.find(']');
        if (closing_bracket == std::string::npos) {
            throw std::invalid_argument("invalid bracketed backend host");
        }
        const auto host = backend.substr(1, closing_bracket - 1);
        if (closing_bracket + 1 == backend.size()) {
            return {host, "80"};
        }
        if (backend[closing_bracket + 1] != ':' || closing_bracket + 2 == backend.size()) {
            throw std::invalid_argument("invalid backend port");
        }
        return {host, backend.substr(closing_bracket + 2)};
    }

    const auto colon = backend.rfind(':');
    if (colon != std::string::npos && backend.find(':') == colon) {
        if (colon == 0 || colon + 1 == backend.size()) {
            throw std::invalid_argument("invalid backend host or port");
        }
        return {backend.substr(0, colon), backend.substr(colon + 1)};
    }
    return {backend, "80"};
}

} // namespace core
