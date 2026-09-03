#pragma once

#include <boost/asio/ip/tcp.hpp>

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>

namespace slam {

using tcp = boost::asio::ip::tcp;

// A bounded queue that transfers accepted sockets from the listener to worker
// threads. stop() lets queued work drain before workers exit.
class InboundConnectionQueue {
  public:
    explicit InboundConnectionQueue(std::size_t capacity);

    [[nodiscard]] bool try_push(tcp::socket socket);
    [[nodiscard]] bool pop(tcp::socket& socket);
    void stop() noexcept;

  private:
    std::size_t capacity_;
    std::deque<tcp::socket> connections_;
    std::mutex mutex_;
    std::condition_variable ready_;
    bool stopped_ = false;
};

} // namespace slam
