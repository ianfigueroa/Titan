#include <gtest/gtest.h>
#include "orderbook/order_book.hpp"
#include "binance/types.hpp"

using namespace titan;
using namespace titan::binance;

class OrderBookTest : public ::testing::Test {
protected:
    OrderBook book{5};  // 5 levels for imbalance calculation

    DepthSnapshot make_snapshot(SequenceId last_id,
                                std::vector<PriceLevel> bids,
                                std::vector<PriceLevel> asks) {
        return DepthSnapshot{
            .last_update_id = last_id,
            .event_time = 0,
            .symbol = "BTCUSDT",
            .bids = std::move(bids),
            .asks = std::move(asks)
        };
    }

    DepthUpdate make_update(SequenceId first_id, SequenceId final_id, SequenceId prev_id,
                            std::vector<PriceLevel> bids,
                            std::vector<PriceLevel> asks) {
        return DepthUpdate{
            .event_type = "depthUpdate",
            .event_time = 0,
            .transaction_time = 0,
            .symbol = "BTCUSDT",
            .first_update_id = first_id,
            .final_update_id = final_id,
            .prev_final_update_id = prev_id,
            .bids = std::move(bids),
            .asks = std::move(asks)
        };
    }
};

TEST_F(OrderBookTest, ApplySnapshot) {
    auto snapshot = make_snapshot(1000,
        {{42150.0, 1.5}, {42149.0, 2.0}, {42148.0, 0.5}},  // bids
        {{42151.0, 1.0}, {42152.0, 1.5}}                   // asks
    );

    auto metrics = book.apply_snapshot(snapshot);

    EXPECT_DOUBLE_EQ(metrics.best_bid, 42150.0);
    EXPECT_DOUBLE_EQ(metrics.best_ask, 42151.0);
    EXPECT_DOUBLE_EQ(metrics.best_bid_qty, 1.5);
    EXPECT_DOUBLE_EQ(metrics.best_ask_qty, 1.0);
    EXPECT_DOUBLE_EQ(metrics.spread, 1.0);
    EXPECT_DOUBLE_EQ(metrics.mid_price, 42150.5);
    EXPECT_EQ(book.last_update_id(), 1000u);
}

TEST_F(OrderBookTest, ApplyUpdate) {
    // First apply a snapshot
    auto snapshot = make_snapshot(1000,
        {{42150.0, 1.5}, {42149.0, 2.0}},
        {{42151.0, 1.0}, {42152.0, 1.5}}
    );
    (void)book.apply_snapshot(snapshot);

    // Then apply an update
    auto update = make_update(1001, 1002, 1000,
        {{42150.0, 2.0}},  // Update best bid quantity
        {{42151.0, 0.5}}   // Update best ask quantity
    );

    auto metrics = book.apply_update(update);

    EXPECT_DOUBLE_EQ(metrics.best_bid, 42150.0);
    EXPECT_DOUBLE_EQ(metrics.best_bid_qty, 2.0);  // Updated
    EXPECT_DOUBLE_EQ(metrics.best_ask_qty, 0.5);  // Updated
    EXPECT_EQ(book.last_update_id(), 1002u);
}

TEST_F(OrderBookTest, RemoveLevelOnZeroQuantity) {
    auto snapshot = make_snapshot(1000,
        {{42150.0, 1.5}, {42149.0, 2.0}},
        {{42151.0, 1.0}, {42152.0, 1.5}}
    );
    (void)book.apply_snapshot(snapshot);

    // Remove best bid by setting quantity to 0
    auto update = make_update(1001, 1002, 1000,
        {{42150.0, 0.0}},  // Remove this level
        {}
    );

    auto metrics = book.apply_update(update);

    // Best bid should now be the next level
    EXPECT_DOUBLE_EQ(metrics.best_bid, 42149.0);
    EXPECT_DOUBLE_EQ(metrics.best_bid_qty, 2.0);
}

TEST_F(OrderBookTest, AddNewPriceLevel) {
    auto snapshot = make_snapshot(1000,
        {{42150.0, 1.5}},
        {{42152.0, 1.0}}
    );
    (void)book.apply_snapshot(snapshot);

    // Add a new best ask
    auto update = make_update(1001, 1002, 1000,
        {},
        {{42151.0, 0.8}}  // New best ask
    );

    auto metrics = book.apply_update(update);

    EXPECT_DOUBLE_EQ(metrics.best_ask, 42151.0);
    EXPECT_DOUBLE_EQ(metrics.best_ask_qty, 0.8);
    EXPECT_DOUBLE_EQ(metrics.spread, 1.0);  // 42151 - 42150
}

TEST_F(OrderBookTest, SpreadInBasisPoints) {
    auto snapshot = make_snapshot(1000,
        {{42150.0, 1.5}},
        {{42151.0, 1.0}}
    );

    auto metrics = book.apply_snapshot(snapshot);

    // Spread = 1.0, mid = 42150.5
    // Spread in bps = (1.0 / 42150.5) * 10000 ≈ 2.37
    EXPECT_NEAR(metrics.spread_bps, 2.37, 0.01);
}

TEST_F(OrderBookTest, ImbalanceCalculation) {
    // More bid volume = positive imbalance
    auto snapshot = make_snapshot(1000,
        {{42150.0, 10.0}, {42149.0, 10.0}, {42148.0, 10.0}},  // 30 total
        {{42151.0, 5.0}, {42152.0, 5.0}, {42153.0, 5.0}}      // 15 total
    );

    auto metrics = book.apply_snapshot(snapshot);

    // Imbalance = (bid_vol - ask_vol) / (bid_vol + ask_vol)
    // = (30 - 15) / (30 + 15) = 15 / 45 = 0.333...
    EXPECT_NEAR(metrics.imbalance, 0.333, 0.01);
}

TEST_F(OrderBookTest, NegativeImbalance) {
    // More ask volume = negative imbalance
    auto snapshot = make_snapshot(1000,
        {{42150.0, 5.0}},   // 5 total bid
        {{42151.0, 15.0}}   // 15 total ask
    );

    auto metrics = book.apply_snapshot(snapshot);

    // Imbalance = (5 - 15) / (5 + 15) = -10 / 20 = -0.5
    EXPECT_NEAR(metrics.imbalance, -0.5, 0.01);
}

TEST_F(OrderBookTest, SequenceGapDetection) {
    (void)book.apply_snapshot(make_snapshot(1000, {}, {}));

    // Valid sequence: prev = 1000
    EXPECT_FALSE(book.has_sequence_gap(1001, 1000));

    // Gap: expected prev = 1000, but got 999
    EXPECT_TRUE(book.has_sequence_gap(1001, 999));

    // Gap: expected prev = 1000, but got 1001 (skipped)
    EXPECT_TRUE(book.has_sequence_gap(1002, 1001));
}

TEST_F(OrderBookTest, ClearResetsState) {
    auto snapshot = make_snapshot(1000,
        {{42150.0, 1.5}},
        {{42151.0, 1.0}}
    );
    (void)book.apply_snapshot(snapshot);
    EXPECT_EQ(book.last_update_id(), 1000u);

    book.clear();

    EXPECT_EQ(book.last_update_id(), 0u);
    auto metrics = book.snapshot();
    EXPECT_DOUBLE_EQ(metrics.best_bid, 0.0);
    EXPECT_DOUBLE_EQ(metrics.best_ask, 0.0);
}

TEST_F(OrderBookTest, BidsSortedDescending) {
    auto snapshot = make_snapshot(1000,
        {{42148.0, 1.0}, {42150.0, 1.0}, {42149.0, 1.0}},  // Out of order
        {{42151.0, 1.0}}
    );

    auto metrics = book.apply_snapshot(snapshot);

    // Best bid should be highest price
    EXPECT_DOUBLE_EQ(metrics.best_bid, 42150.0);
}

TEST_F(OrderBookTest, AsksSortedAscending) {
    auto snapshot = make_snapshot(1000,
        {{42150.0, 1.0}},
        {{42153.0, 1.0}, {42151.0, 1.0}, {42152.0, 1.0}}  // Out of order
    );

    auto metrics = book.apply_snapshot(snapshot);

    // Best ask should be lowest price
    EXPECT_DOUBLE_EQ(metrics.best_ask, 42151.0);
}

TEST_F(OrderBookTest, EmptyBookHandledGracefully) {
    auto metrics = book.snapshot();

    EXPECT_DOUBLE_EQ(metrics.best_bid, 0.0);
    EXPECT_DOUBLE_EQ(metrics.best_ask, 0.0);
    EXPECT_DOUBLE_EQ(metrics.spread, 0.0);
    EXPECT_DOUBLE_EQ(metrics.mid_price, 0.0);
    EXPECT_DOUBLE_EQ(metrics.imbalance, 0.0);
}

TEST_F(OrderBookTest, CachedBestIteratorsPerformance) {
    // Apply snapshot with many levels
    std::vector<PriceLevel> bids, asks;
    for (int i = 0; i < 100; ++i) {
        bids.emplace_back(42150.0 - i, 1.0);
        asks.emplace_back(42151.0 + i, 1.0);
    }

    (void)book.apply_snapshot(make_snapshot(1000, bids, asks));

    // Multiple snapshots should all be O(1) for best bid/ask access
    for (int i = 0; i < 1000; ++i) {
        auto metrics = book.snapshot();
        EXPECT_DOUBLE_EQ(metrics.best_bid, 42150.0);
        EXPECT_DOUBLE_EQ(metrics.best_ask, 42151.0);
    }
}
