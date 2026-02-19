#include <benchmark/benchmark.h>
#include <array>
#include <cstdint>
#include "queue/spsc_queue.hpp"

using namespace titan;

// Benchmark single push/pop cycle
static void BM_SpscQueuePushPop(benchmark::State& state) {
    SpscQueue<int, 65536> queue;
    for (auto _ : state) {
        queue.try_push(42);
        benchmark::DoNotOptimize(queue.try_pop());
    }
}
BENCHMARK(BM_SpscQueuePushPop);

// Benchmark batch push then batch pop
static void BM_SpscQueueBatchPushPop(benchmark::State& state) {
    constexpr std::size_t kBatchSize = 1000;
    SpscQueue<int, 65536> queue;

    for (auto _ : state) {
        // Push batch
        for (std::size_t i = 0; i < kBatchSize; ++i) {
            queue.try_push(static_cast<int>(i));
        }
        // Pop batch
        for (std::size_t i = 0; i < kBatchSize; ++i) {
            benchmark::DoNotOptimize(queue.try_pop());
        }
    }
    state.SetItemsProcessed(state.iterations() * kBatchSize * 2);
}
BENCHMARK(BM_SpscQueueBatchPushPop);

// Benchmark with larger message type (simulating EngineMessage)
struct LargeMessage {
    std::uint64_t timestamp;
    double price;
    double quantity;
    std::array<char, 32> symbol;
    std::uint64_t sequence_id;
    bool is_buyer;
    char padding[7];
};
// Ensure structure is reasonably sized (exact size may vary by platform due to padding)
static_assert(sizeof(LargeMessage) >= 64, "LargeMessage should be at least 64 bytes");

static void BM_SpscQueueLargeMessage(benchmark::State& state) {
    SpscQueue<LargeMessage, 65536> queue;
    LargeMessage msg{
        .timestamp = 1699500000000,
        .price = 42150.50,
        .quantity = 1.5,
        .symbol = {"BTCUSDT"},
        .sequence_id = 1000,
        .is_buyer = true,
        .padding = {}
    };

    for (auto _ : state) {
        queue.try_push(msg);
        benchmark::DoNotOptimize(queue.try_pop());
    }
    state.SetBytesProcessed(state.iterations() * sizeof(LargeMessage) * 2);
}
BENCHMARK(BM_SpscQueueLargeMessage);

// Benchmark queue capacity utilization
static void BM_SpscQueueNearFull(benchmark::State& state) {
    constexpr std::size_t kCapacity = 1024;
    SpscQueue<int, kCapacity> queue;

    // Fill queue to 90% capacity
    for (std::size_t i = 0; i < kCapacity * 9 / 10; ++i) {
        queue.try_push(static_cast<int>(i));
    }

    for (auto _ : state) {
        // Pop one and push one (maintains near-full state)
        benchmark::DoNotOptimize(queue.try_pop());
        queue.try_push(42);
    }
}
BENCHMARK(BM_SpscQueueNearFull);

// Benchmark empty queue pop (should be fast)
static void BM_SpscQueueEmptyPop(benchmark::State& state) {
    SpscQueue<int, 1024> queue;

    for (auto _ : state) {
        benchmark::DoNotOptimize(queue.try_pop());
    }
}
BENCHMARK(BM_SpscQueueEmptyPop);

// Benchmark full queue push (should be fast rejection)
static void BM_SpscQueueFullPush(benchmark::State& state) {
    constexpr std::size_t kCapacity = 1024;
    SpscQueue<int, kCapacity> queue;

    // Fill completely
    while (queue.try_push(42)) {}

    for (auto _ : state) {
        benchmark::DoNotOptimize(queue.try_push(42));
    }
}
BENCHMARK(BM_SpscQueueFullPush);

BENCHMARK_MAIN();
