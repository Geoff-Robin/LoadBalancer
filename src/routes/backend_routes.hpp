#pragma once

#include "core/backend_registry.hpp"
#include "slam/router.hpp"

#include <memory>

namespace routes {

[[nodiscard]] slam::Router make_backend_routes(std::shared_ptr<core::BackendRegistry> registry);

} // namespace routes
