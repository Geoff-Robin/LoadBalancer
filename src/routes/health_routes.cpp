#include "routes/health_routes.hpp"

namespace routes {

slam::Router make_health_routes() {
    slam::Router router;
    router.get("/", [](const slam::Request& request) {
        return slam::json_response(slam::http::status::ok, request,
                                   {{"message", "Hello from slam!"}});
    });
    return router;
}

} // namespace routes
