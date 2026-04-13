/**
 * @file token_queue.cpp
 * @brief Implementation of the thread-safe @ref TokenQueue.
 */

#include "engine/token_queue.hpp"

namespace chronicle {

void TokenQueue::push(std::string token) {
    std::lock_guard lock(mutex_);
    queue_.push_back(std::move(token));
}

std::optional<std::string> TokenQueue::try_pop() {
    std::lock_guard lock(mutex_);
    if (queue_.empty()) {
        return std::nullopt;
    }
    std::string token = std::move(queue_.front());
    queue_.pop_front();
    return token;
}

void TokenQueue::signal_done() { done_.store(true, std::memory_order_release); }

bool TokenQueue::is_done() const { return done_.load(std::memory_order_acquire); }

void TokenQueue::reset() {
    std::lock_guard lock(mutex_);
    queue_.clear();
    done_.store(false, std::memory_order_release);
}

} // namespace chronicle
