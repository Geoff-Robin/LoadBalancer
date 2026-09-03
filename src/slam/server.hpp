#pragma once

#include "slam/connection_queue.hpp"
#include "slam/router.hpp"
#include <boost/asio/io_context.hpp>
#include <cstdint>
#include <thread>
#include <vector>

namespace slam {
namespace net = boost::asio;

struct InboundWorkerConfig {
    std::size_t worker_count = 4;
    std::size_t max_pending_connections = 256;
};

struct ServerConfig {
    std::uint16_t port = 3000;
    InboundWorkerConfig inbound_workers{};
};

class Server {
  public:
    explicit Server(std::uint16_t port = 3000);
    explicit Server(ServerConfig config);
    Router& router() noexcept;
    const Router& router() const noexcept;
    void add_router(Router router);
    [[nodiscard]] std::uint16_t port() const;
    [[nodiscard]] const InboundWorkerConfig& inbound_worker_config() const noexcept;
    void run();
    void stop() noexcept;

  private:
    void serve(tcp::socket socket);
    void worker_loop();
    [[nodiscard]] Response dispatch(const Request& request) const;
    ServerConfig config_;
    net::io_context ioc_;
    tcp::acceptor acceptor_;
    Router router_;
    std::vector<Router> routers_;
    InboundConnectionQueue connection_queue_;
    std::vector<std::thread> workers_;
};
} // namespace slam
