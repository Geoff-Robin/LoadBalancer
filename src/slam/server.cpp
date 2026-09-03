#include "slam/server.hpp"
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <spdlog/spdlog.h>

#include <stdexcept>
#include <utility>

namespace slam {
namespace beast = boost::beast;
namespace http = beast::http;
Server::Server(std::uint16_t port) : Server(ServerConfig{port}) {}

Server::Server(ServerConfig config)
    : config_(std::move(config)), acceptor_(ioc_),
      connection_queue_(config_.inbound_workers.max_pending_connections) {
    if (config_.inbound_workers.worker_count == 0) {
        throw std::invalid_argument("inbound worker count must be greater than zero");
    }

    beast::error_code error;
    const tcp::endpoint endpoint{tcp::v4(), config_.port};
    acceptor_.open(endpoint.protocol(), error);
    if (!error)
        acceptor_.set_option(net::socket_base::reuse_address(true), error);
    if (!error)
        acceptor_.bind(endpoint, error);
    if (!error)
        acceptor_.listen(net::socket_base::max_listen_connections, error);
    if (error) {
        spdlog::error("Failed to start server on port {}: {}", config_.port, error.message());
        throw beast::system_error(error);
    }
    spdlog::info("Listening on http://0.0.0.0:{} with {} inbound worker(s) and a queue of {}",
                 this->port(), config_.inbound_workers.worker_count,
                 config_.inbound_workers.max_pending_connections);
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
void Server::set_fallback(RouteHandler handler) {
    if (!handler) {
        throw std::invalid_argument("fallback handler must not be empty");
    }
    fallback_ = std::move(handler);
}
std::uint16_t Server::port() const {
    return acceptor_.local_endpoint().port();
}
const InboundWorkerConfig& Server::inbound_worker_config() const noexcept {
    return config_.inbound_workers;
}
void Server::run() {
    workers_.reserve(config_.inbound_workers.worker_count);
    for (std::size_t index = 0; index < config_.inbound_workers.worker_count; ++index) {
        workers_.emplace_back(&Server::worker_loop, this);
    }

    beast::error_code accept_error;
    while (acceptor_.is_open()) {
        beast::error_code error;
        tcp::socket socket{ioc_};
        acceptor_.accept(socket, error);
        if (error == net::error::operation_aborted || !acceptor_.is_open())
            break;
        if (error) {
            spdlog::error("Failed to accept connection: {}", error.message());
            accept_error = error;
            break;
        }
        if (!connection_queue_.try_push(std::move(socket))) {
            spdlog::warn("Rejecting connection because the inbound connection queue is full");
        }
    }

    connection_queue_.stop();
    for (auto& worker : workers_) {
        worker.join();
    }
    workers_.clear();

    if (accept_error) {
        throw beast::system_error(accept_error);
    }
}
void Server::stop() noexcept {
    connection_queue_.stop();
    beast::error_code ignored;
    acceptor_.close(ignored);
}
void Server::worker_loop() {
    tcp::socket socket{ioc_};
    while (connection_queue_.pop(socket)) {
        try {
            serve(std::move(socket));
        } catch (const std::exception& error) {
            spdlog::error("Inbound connection worker failed: {}", error.what());
        }
        socket = tcp::socket{ioc_};
    }
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
    if (fallback_) {
        return fallback_(request);
    }
    return json_response(http::status::not_found, request, {{"error", "not_found"}});
}
} // namespace slam
