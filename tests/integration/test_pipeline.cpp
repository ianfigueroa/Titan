#include <gtest/gtest.h>

#include "binance/types.hpp"
#include "core/messages.hpp"
#include "core/types.hpp"
#include "orderbook/order_book.hpp"
#include "queue/spsc_queue.hpp"
#include "trade/vwap_calculator.hpp"

#include <chrono>
#include <thread>
#include <variant>

using namespace titan;
using namespace titan::binance;

namespace {

// Helper to create a PriceLevel from double values
PriceLevel make_level(double price, double qty) {
    return PriceLevel{FixedPrice(price), qty};
}

// Helper to create price levels from initializer list of pairs
std::vector<PriceLevel> make_levels(std::initializer_list<std::pair<double, double>> pairs) {
    std::vector<PriceLevel> result;
    result.reserve(pairs.size());
    for (const auto& [price, qty] : pairs) {
        result.push_back(make_level(price, qty));
    }
    return result;
}

}  // namespace

// ============================================================================
// Pipeline Integration Tests
// ============================================================================

class PipelineIntegrationTest : public ::testing::Test {
protected:
    static constexpr std::size_t kQueueCapacity = 1024;

    DepthSnapshot make_snapshot(SequenceId last_id,
                                std::initializer_list<std::pair<double, double>> bids,
                                std::initializer_list<std::pair<double, double>> asks) {
        return DepthSnapshot{
            .last_update_id = last_id,
            .event_time = 0,
            .symbol = "BTCUSDT",
            .bids = make_levels(bids),
            .asks = make_levels(asks)
        };
    }

    DepthUpdate make_update(SequenceId first_id, SequenceId final_id, SequenceId prev_id,
                            std::initializer_list<std::pair<double, double>> bids,
                            std::initializer_list<std::pair<double, double>> asks) {
        return DepthUpdate{
            .event_type = "depthUpdate",
            .event_time = 0,
            .transaction_time = 0,
            .symbol = "BTCUSDT",
            .first_update_id = first_id,
            .final_update_id = final_id,
            .prev_final_update_id = prev_id,
            .bids = make_levels(bids),
            .asks = make_levels(asks)
        };
    }

    AggTrade make_trade(TradeId id, double price, double qty, bool is_buyer_maker) {
        return AggTrade{
            .event_type = "aggTrade",
            .event_time = 1699500000000 + id,
            .symbol = "BTCUSDT",
            .agg_trade_id = id,
            .price = price,
            .quantity = qty,
            .first_trade_id = id,
            .last_trade_id = id,
            .trade_time = 1699500000000 + id,
            .is_buyer_maker = is_buyer_maker
        };
    }
};

TEST_F(PipelineIntegrationTest, DepthUpdateFlowsToOrderBook) {
    // Arrange: Create queue and order book
    SpscQueue<EngineMessage, kQueueCapacity> queue;
    OrderBook book(5);

    // Apply initial snapshot
    auto snapshot = make_snapshot(1000,
        {{42150.0, 1.5}, {42149.0, 2.0}},
        {{42151.0, 1.0}, {42152.0, 1.5}}
    );
    (void)book.apply_snapshot(snapshot);

    // Create depth update message
    auto update = make_update(1001, 1002, 1000,
        {{42150.0, 2.5}},  // Update best bid quantity
        {{42151.0, 0.8}}   // Update best ask quantity
    );
    DepthUpdateMsg msg{
        .data = update,
        .received_at = std::chrono::steady_clock::now()
    };

    // Act: Push message through queue
    ASSERT_TRUE(queue.try_push(EngineMessage{msg}));

    // Pop and process
    auto popped = queue.try_pop();
    ASSERT_TRUE(popped.has_value());

    // Extract and apply the update
    auto* update_msg = std::get_if<DepthUpdateMsg>(&*popped);
    ASSERT_NE(update_msg, nullptr);

    auto metrics = book.apply_update(update_msg->data);

    // Assert: Book reflects update
    EXPECT_DOUBLE_EQ(metrics.best_bid, 42150.0);
    EXPECT_DOUBLE_EQ(metrics.best_bid_qty, 2.5);  // Updated from 1.5
    EXPECT_DOUBLE_EQ(metrics.best_ask, 42151.0);
    EXPECT_DOUBLE_EQ(metrics.best_ask_qty, 0.8);  // Updated from 1.0
    EXPECT_EQ(book.last_update_id(), 1002u);
}

TEST_F(PipelineIntegrationTest, TradeUpdateFlowsToVWAP) {
    // Arrange: Create queue and VWAP calculator
    SpscQueue<EngineMessage, kQueueCapacity> queue;
    VwapCalculator vwap(100);

    // Create trade messages
    auto trade1 = make_trade(1, 42150.0, 1.0, false);
    auto trade2 = make_trade(2, 42160.0, 2.0, false);

    AggTradeMsg msg1{
        .data = trade1,
        .received_at = std::chrono::steady_clock::now()
    };
    AggTradeMsg msg2{
        .data = trade2,
        .received_at = std::chrono::steady_clock::now()
    };

    // Act: Push trades through queue
    ASSERT_TRUE(queue.try_push(EngineMessage{msg1}));
    ASSERT_TRUE(queue.try_push(EngineMessage{msg2}));

    // Pop and process
    auto popped1 = queue.try_pop();
    ASSERT_TRUE(popped1.has_value());

    auto* trade_msg1 = std::get_if<AggTradeMsg>(&*popped1);
    ASSERT_NE(trade_msg1, nullptr);
    (void)vwap.add_trade(trade_msg1->data.price, trade_msg1->data.quantity);

    auto popped2 = queue.try_pop();
    ASSERT_TRUE(popped2.has_value());

    auto* trade_msg2 = std::get_if<AggTradeMsg>(&*popped2);
    ASSERT_NE(trade_msg2, nullptr);
    double result = vwap.add_trade(trade_msg2->data.price, trade_msg2->data.quantity);

    // Assert: VWAP calculated correctly
    // VWAP = (42150*1 + 42160*2) / (1 + 2) = 126470 / 3 = 42156.67
    EXPECT_NEAR(result, 42156.67, 0.01);
    EXPECT_EQ(vwap.trade_count(), 2u);
    EXPECT_DOUBLE_EQ(vwap.total_volume(), 3.0);
}

TEST_F(PipelineIntegrationTest, SequenceGapTriggersResync) {
    // Arrange: Order book with initial state
    OrderBook book(5);
    auto snapshot = make_snapshot(1000,
        {{42150.0, 1.5}},
        {{42151.0, 1.0}}
    );
    (void)book.apply_snapshot(snapshot);

    // Create update with sequence gap (prev_id doesn't match book's last_update_id)
    auto gapped_update = make_update(1005, 1006, 1004,  // prev=1004, but book has 1000
        {{42150.0, 2.0}},
        {}
    );

    // Act & Assert: Detect the gap
    EXPECT_TRUE(book.has_sequence_gap(
        gapped_update.first_update_id,
        gapped_update.prev_final_update_id
    ));

    // Simulate creating a SequenceGap message
    SequenceGap gap{
        .expected = book.last_update_id(),
        .received = gapped_update.prev_final_update_id,
        .detected_at = std::chrono::steady_clock::now()
    };

    EXPECT_EQ(gap.expected, 1000u);
    EXPECT_EQ(gap.received, 1004u);

    // After gap detection, would trigger resync (clear and re-snapshot)
    book.clear();
    EXPECT_EQ(book.last_update_id(), 0u);

    // Apply new snapshot to resync
    auto resync_snapshot = make_snapshot(1010,
        {{42155.0, 1.0}},
        {{42156.0, 0.5}}
    );
    auto metrics = book.apply_snapshot(resync_snapshot);

    EXPECT_DOUBLE_EQ(metrics.best_bid, 42155.0);
    EXPECT_EQ(book.last_update_id(), 1010u);
}

TEST_F(PipelineIntegrationTest, QueueOverflowHandled) {
    // Arrange: Create a small queue to test overflow
    constexpr std::size_t kSmallCapacity = 4;
    SpscQueue<EngineMessage, kSmallCapacity> queue;

    // Create messages to fill the queue
    auto snapshot = make_snapshot(1000, {{42150.0, 1.0}}, {{42151.0, 1.0}});
    SnapshotMsg msg{
        .data = snapshot,
        .received_at = std::chrono::steady_clock::now()
    };

    // Act: Fill the queue completely
    std::size_t pushed = 0;
    for (std::size_t i = 0; i < kSmallCapacity + 2; ++i) {
        if (queue.try_push(EngineMessage{msg})) {
            ++pushed;
        }
    }

    // Assert: Queue rejected some messages (overflow handled gracefully)
    // The queue should hold kSmallCapacity - 1 elements (ring buffer behavior)
    EXPECT_LE(pushed, kSmallCapacity);
    EXPECT_GT(pushed, 0u);

    // Queue should not crash or corrupt data
    auto popped = queue.try_pop();
    ASSERT_TRUE(popped.has_value());

    auto* snapshot_msg = std::get_if<SnapshotMsg>(&*popped);
    ASSERT_NE(snapshot_msg, nullptr);
    EXPECT_EQ(snapshot_msg->data.last_update_id, 1000u);
}

TEST_F(PipelineIntegrationTest, MixedMessageTypes) {
    // Arrange: Queue with mixed message types
    SpscQueue<EngineMessage, kQueueCapacity> queue;

    auto snapshot = make_snapshot(1000, {{42150.0, 1.0}}, {{42151.0, 1.0}});
    auto trade = make_trade(1, 42150.5, 0.5, false);
    auto update = make_update(1001, 1001, 1000, {{42150.0, 1.5}}, {});

    SnapshotMsg snapshot_msg{.data = snapshot, .received_at = std::chrono::steady_clock::now()};
    AggTradeMsg trade_msg{.data = trade, .received_at = std::chrono::steady_clock::now()};
    DepthUpdateMsg update_msg{.data = update, .received_at = std::chrono::steady_clock::now()};

    // Act: Push different message types
    ASSERT_TRUE(queue.try_push(EngineMessage{snapshot_msg}));
    ASSERT_TRUE(queue.try_push(EngineMessage{trade_msg}));
    ASSERT_TRUE(queue.try_push(EngineMessage{update_msg}));

    // Assert: Messages preserved correctly
    auto msg1 = queue.try_pop();
    ASSERT_TRUE(msg1.has_value());
    EXPECT_EQ(message_type_name(*msg1), "Snapshot");

    auto msg2 = queue.try_pop();
    ASSERT_TRUE(msg2.has_value());
    EXPECT_EQ(message_type_name(*msg2), "AggTrade");

    auto msg3 = queue.try_pop();
    ASSERT_TRUE(msg3.has_value());
    EXPECT_EQ(message_type_name(*msg3), "DepthUpdate");

    // Queue should be empty now
    EXPECT_FALSE(queue.try_pop().has_value());
}

TEST_F(PipelineIntegrationTest, ShutdownMessageProcessed) {
    // Arrange
    SpscQueue<EngineMessage, kQueueCapacity> queue;

    // Push some messages followed by shutdown
    auto trade = make_trade(1, 42150.0, 1.0, false);
    AggTradeMsg trade_msg{.data = trade, .received_at = std::chrono::steady_clock::now()};

    ASSERT_TRUE(queue.try_push(EngineMessage{trade_msg}));
    ASSERT_TRUE(queue.try_push(EngineMessage{Shutdown{}}));

    // Act & Assert: Process until shutdown
    bool shutdown_received = false;
    while (auto msg = queue.try_pop()) {
        std::visit([&shutdown_received](const auto& m) {
            using T = std::decay_t<decltype(m)>;
            if constexpr (std::is_same_v<T, Shutdown>) {
                shutdown_received = true;
            }
        }, *msg);
    }

    EXPECT_TRUE(shutdown_received);
}

TEST_F(PipelineIntegrationTest, ConnectionStateTransitions) {
    // Arrange
    SpscQueue<EngineMessage, kQueueCapacity> queue;

    ConnectionLost lost_msg{
        .reason = "Network timeout",
        .occurred_at = std::chrono::steady_clock::now()
    };
    ConnectionRestored restored_msg{
        .occurred_at = std::chrono::steady_clock::now()
    };

    // Act
    ASSERT_TRUE(queue.try_push(EngineMessage{lost_msg}));
    ASSERT_TRUE(queue.try_push(EngineMessage{restored_msg}));

    // Assert
    auto msg1 = queue.try_pop();
    ASSERT_TRUE(msg1.has_value());
    EXPECT_EQ(message_type_name(*msg1), "ConnectionLost");

    auto* lost = std::get_if<ConnectionLost>(&*msg1);
    ASSERT_NE(lost, nullptr);
    EXPECT_EQ(lost->reason, "Network timeout");

    auto msg2 = queue.try_pop();
    ASSERT_TRUE(msg2.has_value());
    EXPECT_EQ(message_type_name(*msg2), "ConnectionRestored");
}

TEST_F(PipelineIntegrationTest, OrderBookAndVWAPTogether) {
    // Integration test: Order book and VWAP working in parallel
    SpscQueue<EngineMessage, kQueueCapacity> queue;
    OrderBook book(5);
    VwapCalculator vwap(100);

    // Initial snapshot
    auto snapshot = make_snapshot(1000,
        {{42150.0, 1.5}, {42149.0, 2.0}},
        {{42151.0, 1.0}, {42152.0, 1.5}}
    );
    SnapshotMsg snapshot_msg{.data = snapshot, .received_at = std::chrono::steady_clock::now()};

    // Trades at various prices
    auto trade1 = make_trade(1, 42151.0, 0.5, false);  // Buy at ask
    auto trade2 = make_trade(2, 42150.0, 1.0, true);   // Sell at bid

    AggTradeMsg trade1_msg{.data = trade1, .received_at = std::chrono::steady_clock::now()};
    AggTradeMsg trade2_msg{.data = trade2, .received_at = std::chrono::steady_clock::now()};

    // Depth update
    auto update = make_update(1001, 1001, 1000,
        {{42150.0, 0.5}},  // Bid qty decreased (trade hit bid)
        {}
    );
    DepthUpdateMsg update_msg{.data = update, .received_at = std::chrono::steady_clock::now()};

    // Push all messages
    ASSERT_TRUE(queue.try_push(EngineMessage{snapshot_msg}));
    ASSERT_TRUE(queue.try_push(EngineMessage{trade1_msg}));
    ASSERT_TRUE(queue.try_push(EngineMessage{trade2_msg}));
    ASSERT_TRUE(queue.try_push(EngineMessage{update_msg}));

    // Process all messages
    while (auto msg = queue.try_pop()) {
        std::visit([&](const auto& m) {
            using T = std::decay_t<decltype(m)>;
            if constexpr (std::is_same_v<T, SnapshotMsg>) {
                (void)book.apply_snapshot(m.data);
            } else if constexpr (std::is_same_v<T, AggTradeMsg>) {
                (void)vwap.add_trade(m.data.price, m.data.quantity);
            } else if constexpr (std::is_same_v<T, DepthUpdateMsg>) {
                (void)book.apply_update(m.data);
            }
        }, *msg);
    }

    // Verify final state
    auto book_state = book.snapshot();
    EXPECT_DOUBLE_EQ(book_state.best_bid, 42150.0);
    EXPECT_DOUBLE_EQ(book_state.best_bid_qty, 0.5);  // Updated
    EXPECT_EQ(book.last_update_id(), 1001u);

    // VWAP = (42151*0.5 + 42150*1.0) / 1.5 = (21075.5 + 42150) / 1.5 = 42150.33
    EXPECT_NEAR(vwap.vwap(), 42150.33, 0.01);
    EXPECT_EQ(vwap.trade_count(), 2u);
}
