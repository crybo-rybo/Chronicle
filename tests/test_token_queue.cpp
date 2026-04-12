#include "engine/token_queue.hpp"
#include <gtest/gtest.h>
#include <thread>
#include <vector>

using namespace chronicle;

TEST(TokenQueueTest, PushThenPop) {
    TokenQueue queue;
    queue.push("hello");
    auto result = queue.try_pop();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "hello");
}

TEST(TokenQueueTest, PopEmptyReturnsNullopt) {
    TokenQueue queue;
    EXPECT_FALSE(queue.try_pop().has_value());
}

TEST(TokenQueueTest, FifoOrder) {
    TokenQueue queue;
    queue.push("a");
    queue.push("b");
    queue.push("c");
    EXPECT_EQ(*queue.try_pop(), "a");
    EXPECT_EQ(*queue.try_pop(), "b");
    EXPECT_EQ(*queue.try_pop(), "c");
    EXPECT_FALSE(queue.try_pop().has_value());
}

TEST(TokenQueueTest, IsDoneInitiallyFalse) {
    TokenQueue queue;
    EXPECT_FALSE(queue.is_done());
}

TEST(TokenQueueTest, SignalDoneSetsFlag) {
    TokenQueue queue;
    queue.signal_done();
    EXPECT_TRUE(queue.is_done());
}

TEST(TokenQueueTest, TryPopAfterDoneOnEmptyReturnsNullopt) {
    TokenQueue queue;
    queue.signal_done();
    EXPECT_FALSE(queue.try_pop().has_value());
}

TEST(TokenQueueTest, CrossThreadNoLoss) {
    TokenQueue queue;
    constexpr int count = 1000;

    std::thread producer([&] {
        for (int i = 0; i < count; ++i) {
            queue.push(std::to_string(i));
        }
        queue.signal_done();
    });

    std::vector<std::string> received;
    while (true) {
        auto token = queue.try_pop();
        if (token) {
            received.push_back(*token);
        } else if (queue.is_done()) {
            // Drain any remaining tokens
            while (auto remaining = queue.try_pop()) {
                received.push_back(*remaining);
            }
            break;
        }
    }

    producer.join();
    ASSERT_EQ(received.size(), count);
    for (int i = 0; i < count; ++i) {
        EXPECT_EQ(received[i], std::to_string(i));
    }
}

TEST(TokenQueueTest, ResetClearsState) {
    TokenQueue queue;
    queue.push("token1");
    queue.push("token2");
    queue.signal_done();

    queue.reset();

    EXPECT_FALSE(queue.is_done());
    EXPECT_FALSE(queue.try_pop().has_value());
}
