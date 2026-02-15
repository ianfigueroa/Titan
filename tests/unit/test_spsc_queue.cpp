#include <gtest/gtest.h>
#include "queue/spsc_queue.hpp"
#include <thread>
#include <vector>
#include <atomic>

using namespace titan;

TEST(SpscQueueTest, PushPopSingleItem) {
    SpscQueue<int, 16> queue;

    EXPECT_TRUE(queue.try_push(42));

    auto result = queue.try_pop();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 42);
}

TEST(SpscQueueTest, PopFromEmptyReturnsNullopt) {
    SpscQueue<int, 16> queue;

    auto result = queue.try_pop();
    EXPECT_FALSE(result.has_value());
}

TEST(SpscQueueTest, FIFOOrdering) {
    SpscQueue<int, 16> queue;

    for (int i = 0; i < 10; ++i) {
        EXPECT_TRUE(queue.try_push(i));
    }

    for (int i = 0; i < 10; ++i) {
        auto result = queue.try_pop();
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(*result, i);
    }
}

TEST(SpscQueueTest, PushFailsWhenFull) {
    SpscQueue<int, 4> queue;  // Capacity = 4

    // Fill the queue
    EXPECT_TRUE(queue.try_push(1));
    EXPECT_TRUE(queue.try_push(2));
    EXPECT_TRUE(queue.try_push(3));
    EXPECT_TRUE(queue.try_push(4));

    // Should fail when full
    EXPECT_FALSE(queue.try_push(5));
}

TEST(SpscQueueTest, PushSucceedsAfterPop) {
    SpscQueue<int, 4> queue;

    // Fill
    EXPECT_TRUE(queue.try_push(1));
    EXPECT_TRUE(queue.try_push(2));
    EXPECT_TRUE(queue.try_push(3));
    EXPECT_TRUE(queue.try_push(4));
    EXPECT_FALSE(queue.try_push(5));

    // Pop one
    auto result = queue.try_pop();
    EXPECT_TRUE(result.has_value());

    // Now push should succeed
    EXPECT_TRUE(queue.try_push(5));
}

TEST(SpscQueueTest, SizeApproximate) {
    SpscQueue<int, 16> queue;

    EXPECT_EQ(queue.size_approx(), 0u);

    queue.try_push(1);
    queue.try_push(2);
    queue.try_push(3);
    EXPECT_EQ(queue.size_approx(), 3u);

    queue.try_pop();
    EXPECT_EQ(queue.size_approx(), 2u);
}

TEST(SpscQueueTest, IsEmpty) {
    SpscQueue<int, 16> queue;

    EXPECT_TRUE(queue.is_empty());

    queue.try_push(1);
    EXPECT_FALSE(queue.is_empty());

    queue.try_pop();
    EXPECT_TRUE(queue.is_empty());
}

TEST(SpscQueueTest, WorksWithMoveOnlyTypes) {
    SpscQueue<std::unique_ptr<int>, 16> queue;

    auto ptr = std::make_unique<int>(42);
    EXPECT_TRUE(queue.try_push(std::move(ptr)));

    auto result = queue.try_pop();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(**result, 42);
}

TEST(SpscQueueTest, WorksWithLargerObjects) {
    struct LargeObject {
        std::array<int, 100> data;
        int id;
    };

    SpscQueue<LargeObject, 8> queue;

    LargeObject obj{};
    obj.id = 123;
    obj.data[0] = 999;

    EXPECT_TRUE(queue.try_push(std::move(obj)));

    auto result = queue.try_pop();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->id, 123);
    EXPECT_EQ(result->data[0], 999);
}

TEST(SpscQueueTest, ThreadSafetySingleProducerSingleConsumer) {
    constexpr int kNumItems = 100000;
    SpscQueue<int, 1024> queue;

    std::atomic<int> sum_produced{0};
    std::atomic<int> sum_consumed{0};
    std::atomic<bool> producer_done{false};

    // Producer thread
    std::thread producer([&]() {
        for (int i = 1; i <= kNumItems; ++i) {
            while (!queue.try_push(i)) {
                std::this_thread::yield();
            }
            sum_produced += i;
        }
        producer_done = true;
    });

    // Consumer thread
    std::thread consumer([&]() {
        int consumed = 0;
        while (consumed < kNumItems) {
            if (auto val = queue.try_pop()) {
                sum_consumed += *val;
                ++consumed;
            } else if (!producer_done) {
                std::this_thread::yield();
            }
        }
    });

    producer.join();
    consumer.join();

    // Verify all items were transferred correctly
    EXPECT_EQ(sum_produced.load(), sum_consumed.load());

    // Sum of 1 to N = N*(N+1)/2
    int expected = kNumItems * (kNumItems + 1) / 2;
    EXPECT_EQ(sum_consumed.load(), expected);
}

TEST(SpscQueueTest, WrapAround) {
    SpscQueue<int, 4> queue;  // Small capacity to test wrap-around quickly

    // Fill and drain multiple times to ensure wrap-around works
    for (int round = 0; round < 10; ++round) {
        for (int i = 0; i < 4; ++i) {
            EXPECT_TRUE(queue.try_push(round * 10 + i));
        }
        for (int i = 0; i < 4; ++i) {
            auto result = queue.try_pop();
            ASSERT_TRUE(result.has_value());
            EXPECT_EQ(*result, round * 10 + i);
        }
    }
}
