#include <benchmark/benchmark.h>
#include "orderbook/order_book.hpp"
#include "binance/types.hpp"

using namespace titan;
using namespace titan::binance;

namespace {

PriceLevel make_level(double price, double qty) {
    return PriceLevel{FixedPrice(price), qty};
}

DepthSnapshot make_snapshot(SequenceId last_id, std::size_t levels) {
    std::vector<PriceLevel> bids, asks;
    bids.reserve(levels);
    asks.reserve(levels);

    double base_price = 42150.0;
    for (std::size_t i = 0; i < levels; ++i) {
        bids.push_back(make_level(base_price - i, 1.0 + i * 0.1));
        asks.push_back(make_level(base_price + 1 + i, 1.0 + i * 0.1));
    }

    return DepthSnapshot{
        .last_update_id = last_id,
        .event_time = 1699500000000,
        .symbol = "BTCUSDT",
        .bids = std::move(bids),
        .asks = std::move(asks)
    };
}

DepthUpdate make_update(SequenceId first_id, SequenceId final_id, SequenceId prev_id,
                        double bid_price, double bid_qty,
                        double ask_price, double ask_qty) {
    return DepthUpdate{
        .event_type = "depthUpdate",
        .event_time = 1699500000000,
        .transaction_time = 1699500000000,
        .symbol = "BTCUSDT",
        .first_update_id = first_id,
        .final_update_id = final_id,
        .prev_final_update_id = prev_id,
        .bids = {make_level(bid_price, bid_qty)},
        .asks = {make_level(ask_price, ask_qty)}
    };
}

}  // namespace

// Benchmark applying a snapshot with varying number of levels
static void BM_OrderBookApplySnapshot(benchmark::State& state) {
    auto levels = static_cast<std::size_t>(state.range(0));
    auto snapshot = make_snapshot(1000, levels);

    for (auto _ : state) {
        OrderBook book(5);
        benchmark::DoNotOptimize(book.apply_snapshot(snapshot));
    }
    state.SetItemsProcessed(state.iterations() * levels * 2);  // bids + asks
}
BENCHMARK(BM_OrderBookApplySnapshot)->Range(10, 1000);

// Benchmark applying incremental updates
static void BM_OrderBookApplyUpdate(benchmark::State& state) {
    OrderBook book(5);
    auto snapshot = make_snapshot(1000, 100);
    (void)book.apply_snapshot(snapshot);

    SequenceId seq = 1001;
    for (auto _ : state) {
        auto update = make_update(seq, seq, seq - 1, 42150.0, 1.5, 42151.0, 1.2);
        benchmark::DoNotOptimize(book.apply_update(update));
        ++seq;
    }
}
BENCHMARK(BM_OrderBookApplyUpdate);

// Benchmark best bid/ask access (should be O(1) with caching)
static void BM_OrderBookSnapshot(benchmark::State& state) {
    OrderBook book(5);
    auto snapshot = make_snapshot(1000, 100);
    (void)book.apply_snapshot(snapshot);

    for (auto _ : state) {
        benchmark::DoNotOptimize(book.snapshot());
    }
}
BENCHMARK(BM_OrderBookSnapshot);

// Benchmark with best bid/ask invalidation
static void BM_OrderBookBestLevelChange(benchmark::State& state) {
    OrderBook book(5);
    auto snapshot = make_snapshot(1000, 100);
    (void)book.apply_snapshot(snapshot);

    SequenceId seq = 1001;
    for (auto _ : state) {
        // Remove best bid (forces cache invalidation)
        auto remove_update = make_update(seq, seq, seq - 1, 42150.0, 0.0, 42151.0, 1.0);
        benchmark::DoNotOptimize(book.apply_update(remove_update));
        ++seq;

        // Add it back
        auto add_update = make_update(seq, seq, seq - 1, 42150.0, 1.0, 42151.0, 1.0);
        benchmark::DoNotOptimize(book.apply_update(add_update));
        ++seq;
    }
}
BENCHMARK(BM_OrderBookBestLevelChange);

// Benchmark sequence gap detection
static void BM_OrderBookSequenceGapCheck(benchmark::State& state) {
    OrderBook book(5);
    auto snapshot = make_snapshot(1000, 100);
    (void)book.apply_snapshot(snapshot);

    for (auto _ : state) {
        benchmark::DoNotOptimize(book.has_sequence_gap(1001, 1000));  // Valid
        benchmark::DoNotOptimize(book.has_sequence_gap(1005, 1004));  // Gap
    }
}
BENCHMARK(BM_OrderBookSequenceGapCheck);

// Benchmark imbalance calculation with varying depth
static void BM_OrderBookImbalanceCalculation(benchmark::State& state) {
    auto imbalance_levels = static_cast<std::size_t>(state.range(0));
    OrderBook book(imbalance_levels);
    auto snapshot = make_snapshot(1000, 100);
    (void)book.apply_snapshot(snapshot);

    for (auto _ : state) {
        benchmark::DoNotOptimize(book.snapshot().imbalance);
    }
}
BENCHMARK(BM_OrderBookImbalanceCalculation)->Range(1, 20);

// Benchmark clear and re-snapshot (simulating resync)
static void BM_OrderBookResync(benchmark::State& state) {
    auto snapshot = make_snapshot(1000, 100);

    for (auto _ : state) {
        OrderBook book(5);
        (void)book.apply_snapshot(snapshot);
        book.clear();
        (void)book.apply_snapshot(snapshot);
    }
}
BENCHMARK(BM_OrderBookResync);

BENCHMARK_MAIN();
