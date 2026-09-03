#include "core/backend_proxy.hpp"
#include "core/backend_endpoint.hpp"

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace core {
namespace {

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

std::string path_from_target(beast::string_view target) {
    return std::string(target.substr(0, target.find('?')));
}

http::response<http::string_body>
error_response(http::status status, const http::request<http::string_body>& request,
               std::string error) {
    http::response<http::string_body> response{status, request.version()};
    response.set(http::field::content_type, "application/json; charset=utf-8");
    response.keep_alive(request.keep_alive());
    response.body() = "{\"error\":\"" + std::move(error) + "\"}";
    response.prepare_payload();
    return response;
}

} // namespace

BackendProxy::BackendProxy(std::shared_ptr<BackendRegistry> registry,
                           std::shared_ptr<BackendHealthMonitor> health_monitor,
                           std::unique_ptr<LoadBalancingStrategy> strategy)
    : registry_(std::move(registry)), health_monitor_(std::move(health_monitor)),
      strategy_(std::move(strategy)) {
    if (!registry_ || !health_monitor_ || !strategy_) {
        throw std::invalid_argument("backend proxy requires registry, health monitor, and strategy");
    }
}

std::optional<Backend>
BackendProxy::select_backend(const http::request<http::string_body>& request) {
    const auto path = path_from_target(request.target());
    const auto all_backends = registry_->backends();
    std::vector<Backend> eligible;
    std::copy_if(all_backends.begin(), all_backends.end(), std::back_inserter(eligible),
                 [this, &path](const Backend& backend) {
                     return std::find(backend.urls.begin(), backend.urls.end(), path) !=
                                backend.urls.end() &&
                            health_monitor_->is_healthy(backend);
                 });
    return strategy_->select(eligible);
}

http::response<http::string_body>
BackendProxy::forward(const http::request<http::string_body>& request) {
    const auto backend = select_backend(request);
    if (!backend) {
        return error_response(http::status::not_found, request, "no_healthy_backend_for_path");
    }

    try {
        const auto endpoint = parse_backend_endpoint(backend->host);
        net::io_context ioc;
        tcp::resolver resolver{ioc};
        beast::tcp_stream stream{ioc};
        const auto endpoints = resolver.resolve(endpoint.host, endpoint.service);
        stream.expires_after(std::chrono::seconds{5});
        stream.connect(endpoints);

        auto outbound = request;
        outbound.set(http::field::host, backend->host);
        outbound.erase(http::field::connection);
        outbound.keep_alive(false);
        http::write(stream, outbound);

        beast::flat_buffer buffer;
        http::response<http::string_body> response;
        http::read(stream, buffer, response);
        response.keep_alive(request.keep_alive() && response.keep_alive());

        beast::error_code ignored;
        stream.socket().shutdown(tcp::socket::shutdown_both, ignored);
        stream.socket().close(ignored);
        return response;
    } catch (const std::exception& error) {
        spdlog::warn("Failed to forward {} to {}: {}", request.target(), backend->host,
                     error.what());
        health_monitor_->mark_unhealthy(backend->host);
        return error_response(http::status::bad_gateway, request, "backend_unavailable");
    }
}

} // namespace core
