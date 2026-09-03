#pragma once

#include <boost/beast/http.hpp>
#include <nlohmann/json.hpp>

#include <functional>
#include <string>
#include <vector>

namespace slam {
namespace http = boost::beast::http;
using Request = http::request<http::string_body>;
using Response = http::response<http::string_body>;
using Json = nlohmann::json;
using RouteHandler = std::function<Response(const Request&)>;

[[nodiscard]] Response json_response(http::status status, const Request& request, const Json& body);

class Router {
public:
    void add(http::verb method, std::string path, RouteHandler handler);
    void get(std::string path, RouteHandler handler);
    void post(std::string path, RouteHandler handler);
    void put(std::string path, RouteHandler handler);
    void erase(std::string path, RouteHandler handler);
    [[nodiscard]] bool matches(const Request& request) const;
    [[nodiscard]] bool matches_path(const Request& request) const;
    [[nodiscard]] Response dispatch(const Request& request) const;
    [[nodiscard]] std::size_t size() const noexcept;
private:
    struct Route { http::verb method; std::string path; RouteHandler handler; };
    std::vector<Route> routes_;
};
} // namespace slam
