#include "routes/backend_routes.hpp"

#include <stdexcept>
#include <utility>

namespace routes {

slam::Router make_backend_routes(std::shared_ptr<core::BackendRegistry> registry) {
    slam::Router router;
    router.post("/backends/register", [registry =
                                           std::move(registry)](const slam::Request& request) {
        try {
            const auto payload = slam::Json::parse(request.body());
            if (!payload.is_object() || !payload.contains("host") || !payload.contains("urls") ||
                !payload.at("host").is_string() || !payload.at("urls").is_array()) {
                return slam::json_response(
                    slam::http::status::bad_request, request,
                    {{"error", "expected JSON object with string 'host' and array 'urls'"}});
            }

            core::Backend backend;
            backend.host = payload.at("host").get<std::string>();
            backend.urls = payload.at("urls").get<std::vector<std::string>>();
            const bool created = registry->register_backend(std::move(backend));

            return slam::json_response(
                created ? slam::http::status::created : slam::http::status::ok, request,
                {{"status", created ? "registered" : "updated"},
                 {"host", payload.at("host")},
                 {"urls", payload.at("urls")}});
        } catch (const slam::Json::exception& error) {
            return slam::json_response(slam::http::status::bad_request, request,
                                       {{"error", "invalid JSON"}, {"details", error.what()}});
        } catch (const std::invalid_argument& error) {
            return slam::json_response(slam::http::status::bad_request, request,
                                       {{"error", error.what()}});
        }
    });
    return router;
}

} // namespace routes
