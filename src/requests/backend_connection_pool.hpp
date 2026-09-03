#pragma once

#include <boost/asio/io_context.hpp>
#include <boost/beast/core/tcp_stream.hpp>

#include <chrono>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>

namespace requests {

namespace net = boost::asio;
namespace beast = boost::beast;

struct BackendPoolConfig {
    std::string host;
    std::string service;
    std::size_t max_connections = 20;
    std::chrono::seconds connect_timeout{5};
};

class BackendConnection {
  public:
    explicit BackendConnection(net::io_context& ioc);

    beast::tcp_stream& stream() noexcept;

  private:
    beast::tcp_stream stream_;
};

// A thread-safe, non-blocking lease pool for HTTP/1.1 backend connections.
// Each lease has exclusive access to one connection. A full pool returns
// std::nullopt immediately so callers can apply backpressure without blocking.
class BackendConnectionPool {
  private:
    class State;

  public:
    class Lease {
      public:
        Lease() = default;
        ~Lease();

        Lease(const Lease&) = delete;
        Lease& operator=(const Lease&) = delete;
        Lease(Lease&& other) noexcept;
        Lease& operator=(Lease&& other) noexcept;

        [[nodiscard]] explicit operator bool() const noexcept;
        beast::tcp_stream& stream() noexcept;

        // Call this when a request fails or the response cannot keep the TCP
        // connection alive. The socket is closed instead of being reused.
        void invalidate() noexcept;

      private:
        friend class BackendConnectionPool;
        Lease(std::shared_ptr<State> state, std::shared_ptr<BackendConnection> connection);
        void release() noexcept;

        std::shared_ptr<State> state_;
        std::shared_ptr<BackendConnection> connection_;
        bool reusable_ = true;
    };

    BackendConnectionPool(net::io_context& ioc, BackendPoolConfig config);

    // Leases an idle connection, creates a connection when below capacity, or
    // returns std::nullopt when all connections are busy.
    [[nodiscard]] std::optional<Lease> try_acquire();
    [[nodiscard]] const BackendPoolConfig& config() const noexcept;

  private:
    std::shared_ptr<State> state_;
};

} // namespace requests
