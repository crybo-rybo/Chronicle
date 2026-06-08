/**
 * @file token_queue.hpp
 * @brief Thread-safe queue for streaming LLM output tokens.
 *
 * @details The harness inference call delivers
 * generated tokens via a callback.  @ref TokenQueue bridges that inference
 * thread with the main game loop: the callback calls @ref push on the
 * inference thread while the main thread periodically calls @ref try_pop to
 * drain buffered tokens and forward them to the renderer.
 *
 * Call @ref reset between conversations to clear residual state.
 */

#pragma once
#include <deque>
#include <mutex>
#include <optional>
#include <string>

namespace chronicle {

/// @brief Thread-safe single-producer / single-consumer token queue.
///
/// @details Intended for the streaming dialogue pipeline:
///
/// - **Inference thread** calls @ref push for each token.
/// - **Main thread** calls @ref try_pop in a polling loop (e.g. via
///   @ref AgentInterface::chat_streaming's poll callback).
///
/// @note Only one producer and one consumer are assumed; the mutex protects
/// the queue deque.
class TokenQueue {
  public:
    /// @brief Append a token to the back of the queue.
    ///
    /// @details Thread-safe.  Called from the harness inference callback.
    /// @param token The token string to enqueue.
    void push(std::string token);

    /// @brief Attempt to remove and return the front token.
    ///
    /// @details Thread-safe.  Returns @c std::nullopt immediately if the queue
    /// is empty; does not block.
    ///
    /// @return The front token if available, otherwise @c std::nullopt.
    std::optional<std::string> try_pop();

    /// @brief Clear the queue.
    ///
    /// @details Must be called on the main thread before starting a new
    /// inference turn to discard any stale tokens from a previous conversation.
    void reset();

  private:
    std::deque<std::string> queue_; ///< Buffered token strings.
    mutable std::mutex mutex_;      ///< Guards access to @c queue_.
};

} // namespace chronicle
