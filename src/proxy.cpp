#include <exception>
#include <memory>
#include <utility>

#include "core/backend_registry.hpp"
#include "slam/server.hpp"
#include <spdlog/spdlog.h>

int main(){
    try{
        slam::Router health_routes;
        health_routes.get("/", [](const slam::Request& request) {
            return slam::json_response(slam::http::status::ok, request,
                {{"message", "Hello from slam!"}});
        });

        auto backends = std::make_shared<core::BackendRegistry>();
        slam::Router registration_routes;
        registration_routes.post("/backends/register", [backends](const slam::Request& request) {
            try {
                const auto payload = slam::Json::parse(request.body());
                if (!payload.is_object() || !payload.contains("host") || !payload.contains("urls") ||
                    !payload.at("host").is_string() || !payload.at("urls").is_array()) {
                    return slam::json_response(slam::http::status::bad_request, request,
                        {{"error", "expected JSON object with string 'host' and array 'urls'"}});
                }

                core::Backend backend;
                backend.host = payload.at("host").get<std::string>();
                backend.urls = payload.at("urls").get<std::vector<std::string>>();
                const bool created = backends->register_backend(std::move(backend));

                return slam::json_response(
                    created ? slam::http::status::created : slam::http::status::ok,
                    request,
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

        slam::Server server{3000};
        server.add_router(std::move(health_routes));
        server.add_router(std::move(registration_routes));
        server.run();
    } catch(const std::exception & e){
        spdlog::critical("Server stopped: {}", e.what());
        return 1;
    }
    return 0;
}
