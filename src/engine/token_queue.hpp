#pragma once
#include <atomic>
#include <deque>
#include <mutex>
#include <optional>
#include <string>

namespace chronicle {

class TokenQueue {
public:
    void push(std::string token);
    std::optional<std::string> try_pop();
    void signal_done();
    bool is_done() const;
    void reset();

private:
    std::deque<std::string> queue_;
    mutable std::mutex mutex_;
    std::atomic<bool> done_{false};
};

} // namespace chronicle
