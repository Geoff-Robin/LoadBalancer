#include "slam/server.hpp"
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <spdlog/spdlog.h>

#include <utility>

namespace slam {
namespace beast = boost::beast;
namespace http = beast::http;
Server::Server(std::uint16_t port) : acceptor_(ioc_) {
    beast::error_code error;
    const tcp::endpoint endpoint{tcp::v4(), port};
    acceptor_.open(endpoint.protocol(), error);
    if (!error)
        acceptor_.set_option(net::socket_base::reuse_address(true), error);
    if (!error)
        acceptor_.bind(endpoint, error);
    if (!error)
        acceptor_.listen(net::socket_base::max_listen_connections, error);
    if (error) {
        spdlog::error("Failed to start server on port {}: {}", port, error.message());
        throw beast::system_error(error);
    }
    spdlog::info("Listening on http://0.0.0.0:{}", this->port());
}
Router& Server::router() noexcept {
    return router_;
}
const Router& Server::router() const noexcept {
    return router_;
}
void Server::add_router(Router router) {
    spdlog::info("Registering router with {} route(s)", router.size());
    routers_.push_back(std::move(router));
}
std::uint16_t Server::port() const {
    return acceptor_.local_endpoint().port();
}
void Server::run() {
    while (acceptor_.is_open()) {
        beast::error_code error;
        tcp::socket socket{ioc_};
        acceptor_.accept(socket, error);
        if (error == net::error::operation_aborted || !acceptor_.is_open())
            return;
        if (error) {
            spdlog::error("Failed to accept connection: {}", error.message());
            throw beast::system_error(error);
        }
        serve(std::move(socket));
    }
}
void Server::stop() noexcept {
    beast::error_code ignored;
    acceptor_.close(ignored);
}
void Server::serve(tcp::socket socket) {
    beast::flat_buffer buffer;
    beast::error_code error;
    for (;;) {
        Request request;
        http::read(socket, buffer, request, error);
        if (error == http::error::end_of_stream)
            break;
        if (error) {
            spdlog::warn("Failed to read request: {}", error.message());
            break;
        }
        auto response = dispatch(request);
        const bool close = response.need_eof();
        spdlog::info("{} {} -> {}", request.method_string(), request.target(),
                     response.result_int());
        http::write(socket, response, error);
        if (error) {
            spdlog::warn("Failed to write response: {}", error.message());
            break;
        }
        if (close)
            break;
    }
    socket.shutdown(tcp::socket::shutdown_send, error);
}
Response Server::dispatch(const Request& request) const {
    if (router_.matches(request))
        return router_.dispatch(request);
    for (const auto& router : routers_) {
        if (router.matches(request))
            return router.dispatch(request);
    }
    if (router_.matches_path(request))
        return json_response(http::status::method_not_allowed, request,
                             {{"error", "method_not_allowed"}});
    for (const auto& router : routers_) {
        if (router.matches_path(request))
            return json_response(http::status::method_not_allowed, request,
                                 {{"error", "method_not_allowed"}});
    }
    return json_response(http::status::not_found, request, {{"error", "not_found"}});
}
} // namespace slam
