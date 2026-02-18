#include <gtest/gtest.h>
#include "binance/message_parser.hpp"
#include <string>

using namespace titan;
using namespace titan::binance;

// Sample JSON messages from Binance Futures WebSocket

const char* DEPTH_UPDATE_JSON = R"({
    "e": "depthUpdate",
    "E": 1699500000000,
    "T": 1699500000001,
    "s": "BTCUSDT",
    "U": 1000000001,
    "u": 1000000010,
    "pu": 1000000000,
    "b": [
        ["42150.50", "1.500"],
        ["42150.00", "2.000"],
        ["42149.50", "0.000"]
    ],
    "a": [
        ["42151.00", "1.200"],
        ["42151.50", "0.800"]
    ]
})";

const char* AGG_TRADE_JSON = R"({
    "e": "aggTrade",
    "E": 1699500000000,
    "s": "BTCUSDT",
    "a": 123456789,
    "p": "42150.75",
    "q": "0.500",
    "f": 100000001,
    "l": 100000005,
    "T": 1699500000002,
    "m": true
})";

const char* DEPTH_SNAPSHOT_JSON = R"({
    "lastUpdateId": 1000000050,
    "E": 1699500000100,
    "T": 1699500000101,
    "bids": [
        ["42150.50", "1.500"],
        ["42150.00", "2.000"]
    ],
    "asks": [
        ["42151.00", "1.200"],
        ["42151.50", "0.800"]
    ]
})";

const char* COMBINED_STREAM_JSON = R"({
    "stream": "btcusdt@depth@100ms",
    "data": {
        "e": "depthUpdate",
        "E": 1699500000000,
        "T": 1699500000001,
        "s": "BTCUSDT",
        "U": 1000000001,
        "u": 1000000010,
        "pu": 1000000000,
        "b": [],
        "a": []
    }
})";

TEST(MessageParserTest, ParseDepthUpdate) {
    auto result = MessageParser::parse_depth_update(DEPTH_UPDATE_JSON);

    ASSERT_TRUE(result.is_ok()) << result.error();

    const auto& update = result.value();
    EXPECT_EQ(update.event_type, "depthUpdate");
    EXPECT_EQ(update.event_time, 1699500000000u);
    EXPECT_EQ(update.transaction_time, 1699500000001u);
    EXPECT_EQ(update.symbol, "BTCUSDT");
    EXPECT_EQ(update.first_update_id, 1000000001u);
    EXPECT_EQ(update.final_update_id, 1000000010u);
    EXPECT_EQ(update.prev_final_update_id, 1000000000u);

    ASSERT_EQ(update.bids.size(), 3u);
    EXPECT_DOUBLE_EQ(update.bids[0].first.to_double(), 42150.50);
    EXPECT_DOUBLE_EQ(update.bids[0].second, 1.500);
    EXPECT_DOUBLE_EQ(update.bids[2].second, 0.0);  // Zero qty = remove level

    ASSERT_EQ(update.asks.size(), 2u);
    EXPECT_DOUBLE_EQ(update.asks[0].first.to_double(), 42151.00);
    EXPECT_DOUBLE_EQ(update.asks[1].second, 0.800);
}

TEST(MessageParserTest, ParseAggTrade) {
    auto result = MessageParser::parse_agg_trade(AGG_TRADE_JSON);

    ASSERT_TRUE(result.is_ok()) << result.error();

    const auto& trade = result.value();
    EXPECT_EQ(trade.event_type, "aggTrade");
    EXPECT_EQ(trade.event_time, 1699500000000u);
    EXPECT_EQ(trade.symbol, "BTCUSDT");
    EXPECT_EQ(trade.agg_trade_id, 123456789u);
    EXPECT_DOUBLE_EQ(trade.price, 42150.75);
    EXPECT_DOUBLE_EQ(trade.quantity, 0.500);
    EXPECT_EQ(trade.first_trade_id, 100000001u);
    EXPECT_EQ(trade.last_trade_id, 100000005u);
    EXPECT_EQ(trade.trade_time, 1699500000002u);
    EXPECT_TRUE(trade.is_buyer_maker);  // Sell aggressor
}

TEST(MessageParserTest, ParseAggTradeBuyAggressor) {
    // is_buyer_maker = false means buy aggressor
    const char* buy_trade = R"({
        "e": "aggTrade",
        "E": 1699500000000,
        "s": "BTCUSDT",
        "a": 123456790,
        "p": "42152.00",
        "q": "1.000",
        "f": 100000010,
        "l": 100000012,
        "T": 1699500000010,
        "m": false
    })";

    auto result = MessageParser::parse_agg_trade(buy_trade);
    ASSERT_TRUE(result.is_ok());
    EXPECT_FALSE(result.value().is_buyer_maker);  // Buy aggressor
}

TEST(MessageParserTest, ParseDepthSnapshot) {
    auto result = MessageParser::parse_depth_snapshot(DEPTH_SNAPSHOT_JSON, "BTCUSDT");

    ASSERT_TRUE(result.is_ok()) << result.error();

    const auto& snapshot = result.value();
    EXPECT_EQ(snapshot.last_update_id, 1000000050u);
    EXPECT_EQ(snapshot.symbol, "BTCUSDT");

    ASSERT_EQ(snapshot.bids.size(), 2u);
    EXPECT_DOUBLE_EQ(snapshot.bids[0].first.to_double(), 42150.50);
    EXPECT_DOUBLE_EQ(snapshot.bids[1].second, 2.000);

    ASSERT_EQ(snapshot.asks.size(), 2u);
    EXPECT_DOUBLE_EQ(snapshot.asks[0].first.to_double(), 42151.00);
}

TEST(MessageParserTest, ParseCombinedStream) {
    auto result = MessageParser::parse_combined_stream(COMBINED_STREAM_JSON);

    ASSERT_TRUE(result.is_ok()) << result.error();

    const auto& stream_msg = result.value();
    EXPECT_EQ(stream_msg.stream, "btcusdt@depth@100ms");
    EXPECT_FALSE(stream_msg.data.empty());
}

TEST(MessageParserTest, IdentifyDepthStream) {
    EXPECT_TRUE(MessageParser::is_depth_stream("btcusdt@depth@100ms"));
    EXPECT_TRUE(MessageParser::is_depth_stream("ethusdt@depth@100ms"));
    EXPECT_FALSE(MessageParser::is_depth_stream("btcusdt@aggTrade"));
    EXPECT_FALSE(MessageParser::is_depth_stream("btcusdt@trade"));
}

TEST(MessageParserTest, IdentifyAggTradeStream) {
    EXPECT_TRUE(MessageParser::is_agg_trade_stream("btcusdt@aggTrade"));
    EXPECT_TRUE(MessageParser::is_agg_trade_stream("ethusdt@aggTrade"));
    EXPECT_FALSE(MessageParser::is_agg_trade_stream("btcusdt@depth@100ms"));
    EXPECT_FALSE(MessageParser::is_agg_trade_stream("btcusdt@trade"));
}

TEST(MessageParserTest, InvalidJsonReturnsError) {
    auto result = MessageParser::parse_depth_update("not valid json");
    EXPECT_TRUE(result.is_err());
}

TEST(MessageParserTest, MissingFieldsReturnsError) {
    // Missing required field "u"
    const char* incomplete = R"({
        "e": "depthUpdate",
        "E": 1699500000000,
        "s": "BTCUSDT",
        "U": 1000000001
    })";

    auto result = MessageParser::parse_depth_update(incomplete);
    EXPECT_TRUE(result.is_err());
}
