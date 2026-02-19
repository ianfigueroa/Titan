#include <benchmark/benchmark.h>
#include "trade/vwap_calculator.hpp"
#include <random>

using namespace titan;

// Benchmark single trade addition
static void BM_VwapAddTrade(benchmark::State& state) {
    VwapCalculator calc(100);

    for (auto _ : state) {
        benchmark::DoNotOptimize(calc.add_trade(42150.0, 1.0));
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_VwapAddTrade);

// Benchmark with window sliding
static void BM_VwapWindowSliding(benchmark::State& state) {
    auto window_size = static_cast<std::size_t>(state.range(0));
    VwapCalculator calc(window_size);

    // Pre-fill the window
    for (std::size_t i = 0; i < window_size; ++i) {
        (void)calc.add_trade(42150.0 + i, 1.0);
    }

    double price = 42150.0;
    for (auto _ : state) {
        benchmark::DoNotOptimize(calc.add_trade(price, 1.0));
        price += 0.01;
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_VwapWindowSliding)->Range(10, 1000);

// Benchmark VWAP retrieval
static void BM_VwapGet(benchmark::State& state) {
    VwapCalculator calc(100);

    // Add some trades
    for (int i = 0; i < 50; ++i) {
        (void)calc.add_trade(42150.0 + i, 1.0 + i * 0.1);
    }

    for (auto _ : state) {
        benchmark::DoNotOptimize(calc.vwap());
    }
}
BENCHMARK(BM_VwapGet);

// Benchmark rolling statistics retrieval
static void BM_VwapRollingStats(benchmark::State& state) {
    VwapCalculator calc(100);

    // Add some trades
    for (int i = 0; i < 50; ++i) {
        (void)calc.add_trade(42150.0 + i, 1.0 + i * 0.1);
    }

    for (auto _ : state) {
        benchmark::DoNotOptimize(calc.rolling_avg_size());
        benchmark::DoNotOptimize(calc.rolling_std_dev());
    }
}
BENCHMARK(BM_VwapRollingStats);

// Benchmark with realistic price/quantity variations
static void BM_VwapRealisticTrades(benchmark::State& state) {
    VwapCalculator calc(100);

    // Pre-fill with some trades
    for (int i = 0; i < 50; ++i) {
        (void)calc.add_trade(42150.0, 1.0);
    }

    std::mt19937 rng(42);
    std::normal_distribution<double> price_dist(42150.0, 10.0);
    std::exponential_distribution<double> qty_dist(1.0);

    for (auto _ : state) {
        double price = price_dist(rng);
        double qty = qty_dist(rng);
        benchmark::DoNotOptimize(calc.add_trade(price, qty));
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_VwapRealisticTrades);

// Benchmark clear and rebuild
static void BM_VwapClearAndRebuild(benchmark::State& state) {
    auto window_size = static_cast<std::size_t>(state.range(0));

    for (auto _ : state) {
        VwapCalculator calc(window_size);
        // Simulate filling the window
        for (std::size_t i = 0; i < window_size; ++i) {
            (void)calc.add_trade(42150.0, 1.0);
        }
        calc.clear();
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_VwapClearAndRebuild)->Range(10, 1000);

// Benchmark total volume tracking
static void BM_VwapTotalVolume(benchmark::State& state) {
    VwapCalculator calc(100);

    // Add some trades
    for (int i = 0; i < 50; ++i) {
        (void)calc.add_trade(42150.0, 1.0 + i * 0.1);
    }

    for (auto _ : state) {
        benchmark::DoNotOptimize(calc.total_volume());
        benchmark::DoNotOptimize(calc.trade_count());
    }
}
BENCHMARK(BM_VwapTotalVolume);

BENCHMARK_MAIN();
