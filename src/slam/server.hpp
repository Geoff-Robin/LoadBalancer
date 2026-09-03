#pragma once

#include "slam/router.hpp"
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <cstdint>
#include <vector>

namespace slam {
namespace net = boost::asio;
using tcp = net::ip::tcp;

class Server {
  public:
    explicit Server(std::uint16_t port = 3000);
    Router& router() noexcept;
    const Router& router() const noexcept;
    void add_router(Router router);
    [[nodiscard]] std::uint16_t port() const;
    void run();
    void stop() noexcept;

  private:
    void serve(tcp::socket socket);
    [[nodiscard]] Response dispatch(const Request& request) const;
    net::io_context ioc_;
    tcp::acceptor acceptor_;
    Router router_;
    std::vector<Router> routers_;
};
} // namespace slam
