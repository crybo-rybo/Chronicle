/**
 * @file token_queue.hpp
 * @brief Thread-safe queue for streaming LLM output tokens.
 *
 * @details Zoo-Keeper's inference runs on its own internal thread and delivers
 * generated tokens via a callback.  @ref TokenQueue bridges that inference
 * thread with the main game loop: the callback calls @ref push on the
 * inference thread while the main thread periodically calls @ref try_pop to
 * drain buffered tokens and forward them to the renderer.
 *
 * The @ref signal_done / @ref is_done pair provides a lightweight termination
 * signal so the main thread knows when inference has finished and it should
 * stop polling.  Call @ref reset between conversations to clear residual
 * state.
 */

#pragma once
#include <atomic>
#include <deque>
#include <mutex>
#include <optional>
#include <string>

namespace chronicle {

/// @brief Thread-safe single-producer / single-consumer token queue.
///
/// @details Intended for the streaming dialogue pipeline:
///
/// - **Inference thread** calls @ref push for each token and
///   @ref signal_done when inference completes.
/// - **Main thread** calls @ref try_pop in a polling loop and checks
///   @ref is_done to determine when to stop.
///
/// @note Only one producer and one consumer are assumed; the mutex protects
/// the queue deque, while the @c done_ flag uses acquire/release ordering
/// sufficient for the single-flag pattern.
class TokenQueue {
public:
    /// @brief Append a token to the back of the queue.
    ///
    /// @details Thread-safe.  Called from the Zoo-Keeper inference callback.
    /// @param token The token string to enqueue.
    void push(std::string token);

    /// @brief Attempt to remove and return the front token.
    ///
    /// @details Thread-safe.  Returns @c std::nullopt immediately if the queue
    /// is empty; does not block.
    ///
    /// @return The front token if available, otherwise @c std::nullopt.
    std::optional<std::string> try_pop();

    /// @brief Signal that all tokens have been pushed.
    ///
    /// @details Called by the inference thread after the last token is pushed.
    /// Sets the done flag with @c memory_order_release so that the consuming
    /// thread can observe it with @c memory_order_acquire via @ref is_done.
    void signal_done();

    /// @brief Test whether @ref signal_done has been called.
    /// @return @c true once the done flag has been set.
    bool is_done() const;

    /// @brief Clear the queue and reset the done flag.
    ///
    /// @details Must be called on the main thread before starting a new
    /// inference turn to discard any stale tokens from a previous conversation.
    void reset();

private:
    std::deque<std::string> queue_; ///< Buffered token strings.
    mutable std::mutex mutex_;      ///< Guards access to @c queue_.
    std::atomic<bool> done_{false}; ///< Set by @ref signal_done.
};

} // namespace chronicle
