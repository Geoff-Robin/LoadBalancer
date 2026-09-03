#include "slam/router.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace slam {
namespace {
std::string path_from_target(boost::beast::string_view target) {
    return std::string(target.substr(0, target.find('?')));
}
} // namespace

Response json_response(http::status status, const Request& request, const Json& body) {
    Response response{status, request.version()};
    response.set(http::field::content_type, "application/json; charset=utf-8");
    response.keep_alive(request.keep_alive());
    response.body() = body.dump();
    response.prepare_payload();
    return response;
}
void Router::add(http::verb method, std::string path, RouteHandler handler) {
    if (path.empty() || path.front() != '/')
        throw std::invalid_argument("route paths must start with '/'");
    if (!handler)
        throw std::invalid_argument("route handler must not be empty");
    if (std::any_of(routes_.begin(), routes_.end(), [method, &path](const Route& route) {
            return route.method == method && route.path == path;
        }))
        throw std::invalid_argument("a route with this method and path already exists");
    routes_.push_back({method, std::move(path), std::move(handler)});
}
void Router::get(std::string path, RouteHandler handler) {
    add(http::verb::get, std::move(path), std::move(handler));
}
void Router::post(std::string path, RouteHandler handler) {
    add(http::verb::post, std::move(path), std::move(handler));
}
void Router::put(std::string path, RouteHandler handler) {
    add(http::verb::put, std::move(path), std::move(handler));
}
void Router::erase(std::string path, RouteHandler handler) {
    add(http::verb::delete_, std::move(path), std::move(handler));
}

Response Router::dispatch(const Request& request) const {
    const auto route =
        std::find_if(routes_.begin(), routes_.end(), [&request](const Route& candidate) {
            return candidate.method == request.method() &&
                   candidate.path == path_from_target(request.target());
        });
    if (route != routes_.end()) {
        auto response = route->handler(request);
        response.version(request.version());
        response.keep_alive(request.keep_alive());
        response.prepare_payload();
        return response;
    }
    if (matches_path(request))
        return json_response(http::status::method_not_allowed, request,
                             {{"error", "method_not_allowed"}});
    return json_response(http::status::not_found, request, {{"error", "not_found"}});
}
bool Router::matches(const Request& request) const {
    return std::any_of(routes_.begin(), routes_.end(), [&request](const Route& route) {
        return route.method == request.method() && route.path == path_from_target(request.target());
    });
}
bool Router::matches_path(const Request& request) const {
    const auto path = path_from_target(request.target());
    return std::any_of(routes_.begin(), routes_.end(),
                       [&path](const Route& route) { return route.path == path; });
}
std::size_t Router::size() const noexcept {
    return routes_.size();
}
} // namespace slam
