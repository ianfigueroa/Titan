#include <gtest/gtest.h>
#include "trade/vwap_calculator.hpp"
#include "trade/alert_detector.hpp"
#include "trade/trade_flow.hpp"
#include <cmath>

using namespace titan;

// ============================================================================
// VwapCalculator Tests
// ============================================================================

TEST(VwapCalculatorTest, SingleTrade) {
    VwapCalculator calc(100);

    double vwap = calc.add_trade(42150.0, 1.0);

    EXPECT_DOUBLE_EQ(vwap, 42150.0);
    EXPECT_DOUBLE_EQ(calc.total_volume(), 1.0);
    EXPECT_EQ(calc.trade_count(), 1u);
}

TEST(VwapCalculatorTest, MultipleTrades) {
    VwapCalculator calc(100);

    // Trade 1: 1.0 @ 42150
    calc.add_trade(42150.0, 1.0);

    // Trade 2: 2.0 @ 42160
    double vwap = calc.add_trade(42160.0, 2.0);

    // VWAP = (42150*1 + 42160*2) / (1 + 2) = 126470 / 3 = 42156.67
    EXPECT_NEAR(vwap, 42156.67, 0.01);
    EXPECT_DOUBLE_EQ(calc.total_volume(), 3.0);
    EXPECT_EQ(calc.trade_count(), 2u);
}

TEST(VwapCalculatorTest, WindowSlidingRemovesOldTrades) {
    VwapCalculator calc(3);  // Small window for testing

    // Fill the window
    calc.add_trade(100.0, 1.0);  // Index 0
    calc.add_trade(200.0, 1.0);  // Index 1
    calc.add_trade(300.0, 1.0);  // Index 2

    // VWAP = (100 + 200 + 300) / 3 = 200
    EXPECT_DOUBLE_EQ(calc.vwap(), 200.0);
    EXPECT_DOUBLE_EQ(calc.total_volume(), 3.0);

    // Add a 4th trade - should push out the first
    double vwap = calc.add_trade(400.0, 1.0);

    // VWAP = (200 + 300 + 400) / 3 = 300
    EXPECT_DOUBLE_EQ(vwap, 300.0);
    EXPECT_DOUBLE_EQ(calc.total_volume(), 3.0);
    EXPECT_EQ(calc.trade_count(), 3u);  // Still 3 trades in window
}

TEST(VwapCalculatorTest, RollingAverageSize) {
    VwapCalculator calc(100);

    calc.add_trade(100.0, 1.0);
    calc.add_trade(100.0, 2.0);
    calc.add_trade(100.0, 3.0);

    // Average size = (1 + 2 + 3) / 3 = 2
    EXPECT_DOUBLE_EQ(calc.rolling_avg_size(), 2.0);
}

TEST(VwapCalculatorTest, RollingStdDev) {
    VwapCalculator calc(100);

    // Add trades with sizes: 1, 2, 3
    calc.add_trade(100.0, 1.0);
    calc.add_trade(100.0, 2.0);
    calc.add_trade(100.0, 3.0);

    // Mean = 2, variance = ((1-2)^2 + (2-2)^2 + (3-2)^2) / 3 = (1 + 0 + 1) / 3 = 0.667
    // Std dev = sqrt(0.667) ≈ 0.816
    EXPECT_NEAR(calc.rolling_std_dev(), 0.816, 0.01);
}

TEST(VwapCalculatorTest, EmptyCalculator) {
    VwapCalculator calc(100);

    EXPECT_DOUBLE_EQ(calc.vwap(), 0.0);
    EXPECT_DOUBLE_EQ(calc.total_volume(), 0.0);
    EXPECT_EQ(calc.trade_count(), 0u);
    EXPECT_DOUBLE_EQ(calc.rolling_avg_size(), 0.0);
    EXPECT_DOUBLE_EQ(calc.rolling_std_dev(), 0.0);
}

// ============================================================================
// AlertDetector Tests
// ============================================================================

TEST(AlertDetectorTest, NoAlertForNormalTrade) {
    AlertDetector detector(2.0);  // 2 std dev threshold

    // Normal trade: 1.5 size, mean=2, std_dev=1 => deviation = -0.5
    auto alert = detector.check_trade(42150.0, 1.5, true, 2.0, 1.0);

    EXPECT_FALSE(alert.has_value());
}

TEST(AlertDetectorTest, AlertForLargeTrade) {
    AlertDetector detector(2.0);

    // Large trade: 5.0 size, mean=2, std_dev=1 => deviation = 3
    auto alert = detector.check_trade(42150.0, 5.0, true, 2.0, 1.0);

    ASSERT_TRUE(alert.has_value());
    EXPECT_DOUBLE_EQ(alert->price, 42150.0);
    EXPECT_DOUBLE_EQ(alert->quantity, 5.0);
    EXPECT_TRUE(alert->is_buy);
    EXPECT_NEAR(alert->deviation, 3.0, 0.01);
}

TEST(AlertDetectorTest, AlertForSellTrade) {
    AlertDetector detector(2.0);

    auto alert = detector.check_trade(42150.0, 10.0, false, 2.0, 1.0);

    ASSERT_TRUE(alert.has_value());
    EXPECT_FALSE(alert->is_buy);  // Sell trade
}

TEST(AlertDetectorTest, NoAlertWithZeroStdDev) {
    AlertDetector detector(2.0);

    // With std_dev = 0, any trade different from mean would be infinite deviation
    // Should handle gracefully
    auto alert = detector.check_trade(42150.0, 5.0, true, 2.0, 0.0);

    // Implementation should return no alert when std_dev is 0 (division by zero guard)
    EXPECT_FALSE(alert.has_value());
}

TEST(AlertDetectorTest, ExactlyAtThresholdNoAlert) {
    AlertDetector detector(2.0);

    // Exactly 2 std devs: 4.0 size, mean=2, std_dev=1 => deviation = 2.0
    // Should NOT trigger (need > threshold, not >=)
    auto alert = detector.check_trade(42150.0, 4.0, true, 2.0, 1.0);

    EXPECT_FALSE(alert.has_value());
}

TEST(AlertDetectorTest, JustOverThresholdTriggers) {
    AlertDetector detector(2.0);

    // Just over 2 std devs
    auto alert = detector.check_trade(42150.0, 4.01, true, 2.0, 1.0);

    EXPECT_TRUE(alert.has_value());
}

// ============================================================================
// TradeFlow Integration Tests
// ============================================================================

TEST(TradeFlowTest, ProcessTrade) {
    Config::Engine config;
    config.vwap_window = 100;
    config.large_trade_std_devs = 2.0;

    TradeFlow flow(config);

    binance::AggTrade trade{
        .event_type = "aggTrade",
        .event_time = 1699500000000,
        .symbol = "BTCUSDT",
        .agg_trade_id = 1,
        .price = 42150.0,
        .quantity = 1.0,
        .first_trade_id = 1,
        .last_trade_id = 1,
        .trade_time = 1699500000000,
        .is_buyer_maker = false  // Buy aggressor
    };

    auto metrics = flow.process_trade(trade);

    EXPECT_DOUBLE_EQ(metrics.vwap, 42150.0);
    EXPECT_EQ(metrics.trade_count, 1u);
    EXPECT_DOUBLE_EQ(metrics.total_buy_volume, 1.0);
    EXPECT_DOUBLE_EQ(metrics.total_sell_volume, 0.0);
}

TEST(TradeFlowTest, BuySellVolumeTracking) {
    Config::Engine config;
    config.vwap_window = 100;
    config.large_trade_std_devs = 2.0;

    TradeFlow flow(config);

    // Buy trade (is_buyer_maker = false means taker was buyer)
    binance::AggTrade buy_trade{
        .event_type = "aggTrade",
        .event_time = 1699500000000,
        .symbol = "BTCUSDT",
        .agg_trade_id = 1,
        .price = 42150.0,
        .quantity = 1.0,
        .first_trade_id = 1,
        .last_trade_id = 1,
        .trade_time = 1699500000000,
        .is_buyer_maker = false
    };

    // Sell trade (is_buyer_maker = true means taker was seller)
    binance::AggTrade sell_trade{
        .event_type = "aggTrade",
        .event_time = 1699500000001,
        .symbol = "BTCUSDT",
        .agg_trade_id = 2,
        .price = 42149.0,
        .quantity = 2.0,
        .first_trade_id = 2,
        .last_trade_id = 2,
        .trade_time = 1699500000001,
        .is_buyer_maker = true
    };

    flow.process_trade(buy_trade);
    auto metrics = flow.process_trade(sell_trade);

    EXPECT_DOUBLE_EQ(metrics.total_buy_volume, 1.0);
    EXPECT_DOUBLE_EQ(metrics.total_sell_volume, 2.0);
    EXPECT_DOUBLE_EQ(metrics.net_flow, -1.0);  // 1 - 2 = -1
}

TEST(TradeFlowTest, LargeTradeAlert) {
    Config::Engine config;
    config.vwap_window = 5;
    config.large_trade_std_devs = 2.0;

    TradeFlow flow(config);

    // Add several small trades to establish baseline
    for (int i = 0; i < 5; ++i) {
        binance::AggTrade small_trade{
            .event_type = "aggTrade",
            .event_time = static_cast<uint64_t>(1699500000000 + i),
            .symbol = "BTCUSDT",
            .agg_trade_id = static_cast<uint64_t>(i + 1),
            .price = 42150.0,
            .quantity = 1.0,
            .first_trade_id = static_cast<uint64_t>(i + 1),
            .last_trade_id = static_cast<uint64_t>(i + 1),
            .trade_time = static_cast<uint64_t>(1699500000000 + i),
            .is_buyer_maker = false
        };
        flow.process_trade(small_trade);
    }

    // Add a very large trade
    binance::AggTrade large_trade{
        .event_type = "aggTrade",
        .event_time = 1699500000010,
        .symbol = "BTCUSDT",
        .agg_trade_id = 10,
        .price = 42150.0,
        .quantity = 100.0,  // Much larger than baseline
        .first_trade_id = 10,
        .last_trade_id = 10,
        .trade_time = 1699500000010,
        .is_buyer_maker = false
    };

    auto metrics = flow.process_trade(large_trade);

    // Should have detected an alert (depending on calculated std dev)
    EXPECT_TRUE(metrics.last_alert.has_value());
}
