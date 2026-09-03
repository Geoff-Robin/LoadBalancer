#include "requests/backend_connection_pool.hpp"

#include <boost/asio/ip/tcp.hpp>
#include <spdlog/spdlog.h>

#include <deque>
#include <mutex>
#include <stdexcept>
#include <utility>

namespace requests {

class BackendConnectionPool::State {
  public:
    State(net::io_context& context, BackendPoolConfig pool_config)
        : ioc(context), config(std::move(pool_config)) {}

    void release(std::shared_ptr<BackendConnection> connection, bool reusable) noexcept {
        std::lock_guard<std::mutex> lock(mutex);
        if (reusable && connection->stream().socket().is_open()) {
            idle_connections.push_back(std::move(connection));
            return;
        }

        beast::error_code error;
        connection->stream().socket().shutdown(net::ip::tcp::socket::shutdown_both, error);
        connection->stream().socket().close(error);
        --connection_count;
    }

    net::io_context& ioc;
    BackendPoolConfig config;
    std::deque<std::shared_ptr<BackendConnection>> idle_connections;
    std::size_t connection_count = 0;
    std::mutex mutex;
};

BackendConnection::BackendConnection(net::io_context& ioc) : stream_(ioc) {}

beast::tcp_stream& BackendConnection::stream() noexcept {
    return stream_;
}

BackendConnectionPool::Lease::Lease(std::shared_ptr<State> state,
                                    std::shared_ptr<BackendConnection> connection)
    : state_(std::move(state)), connection_(std::move(connection)) {}

BackendConnectionPool::Lease::~Lease() {
    release();
}

BackendConnectionPool::Lease::Lease(Lease&& other) noexcept
    : state_(std::move(other.state_)), connection_(std::move(other.connection_)),
      reusable_(other.reusable_) {}

BackendConnectionPool::Lease& BackendConnectionPool::Lease::operator=(Lease&& other) noexcept {
    if (this != &other) {
        release();
        state_ = std::move(other.state_);
        connection_ = std::move(other.connection_);
        reusable_ = other.reusable_;
    }
    return *this;
}

BackendConnectionPool::Lease::operator bool() const noexcept {
    return connection_ != nullptr;
}

beast::tcp_stream& BackendConnectionPool::Lease::stream() noexcept {
    return connection_->stream();
}

void BackendConnectionPool::Lease::invalidate() noexcept {
    reusable_ = false;
}

void BackendConnectionPool::Lease::release() noexcept {
    if (connection_) {
        state_->release(std::move(connection_), reusable_);
        state_.reset();
    }
}

BackendConnectionPool::BackendConnectionPool(net::io_context& ioc, BackendPoolConfig config) {
    if (config.host.empty()) {
        throw std::invalid_argument("backend pool host must not be empty");
    }
    if (config.service.empty()) {
        throw std::invalid_argument("backend pool service must not be empty");
    }
    if (config.max_connections == 0) {
        throw std::invalid_argument("backend pool maximum connections must be greater than zero");
    }
    state_ = std::make_shared<State>(ioc, std::move(config));
}

std::optional<BackendConnectionPool::Lease> BackendConnectionPool::try_acquire() {
    std::shared_ptr<BackendConnection> connection;
    bool requires_connection = false;
    {
        std::lock_guard<std::mutex> lock(state_->mutex);
        if (!state_->idle_connections.empty()) {
            connection = std::move(state_->idle_connections.front());
            state_->idle_connections.pop_front();
        } else if (state_->connection_count < state_->config.max_connections) {
            ++state_->connection_count;
            requires_connection = true;
        } else {
            return std::nullopt;
        }
    }

    if (requires_connection) {
        try {
            connection = std::make_shared<BackendConnection>(state_->ioc);
            net::ip::tcp::resolver resolver(state_->ioc);
            const auto endpoints = resolver.resolve(state_->config.host, state_->config.service);
            connection->stream().expires_after(state_->config.connect_timeout);
            connection->stream().connect(endpoints);
            connection->stream().expires_never();
            spdlog::debug("Opened backend connection to {}:{}", state_->config.host,
                          state_->config.service);
        } catch (...) {
            std::lock_guard<std::mutex> lock(state_->mutex);
            --state_->connection_count;
            throw;
        }
    }

    return Lease{state_, std::move(connection)};
}

const BackendPoolConfig& BackendConnectionPool::config() const noexcept {
    return state_->config;
}

} // namespace requests
