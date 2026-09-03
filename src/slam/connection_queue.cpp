#include "slam/connection_queue.hpp"

#include <stdexcept>
#include <utility>

namespace slam {

InboundConnectionQueue::InboundConnectionQueue(std::size_t capacity) : capacity_(capacity) {
    if (capacity_ == 0) {
        throw std::invalid_argument("inbound connection queue capacity must be greater than zero");
    }
}

bool InboundConnectionQueue::try_push(tcp::socket socket) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopped_ || connections_.size() >= capacity_) {
            return false;
        }
        connections_.push_back(std::move(socket));
    }
    ready_.notify_one();
    return true;
}

bool InboundConnectionQueue::pop(tcp::socket& socket) {
    std::unique_lock<std::mutex> lock(mutex_);
    ready_.wait(lock, [this] { return stopped_ || !connections_.empty(); });
    if (connections_.empty()) {
        return false;
    }

    socket = std::move(connections_.front());
    connections_.pop_front();
    return true;
}

void InboundConnectionQueue::stop() noexcept {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopped_ = true;
    }
    ready_.notify_all();
}

} // namespace slam
