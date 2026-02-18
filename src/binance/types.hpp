#pragma once

#include "core/types.hpp"
#include <string>
#include <vector>
#include <utility>

namespace titan::binance {

/// Price level: (fixed-point price, double quantity)
/// Uses FixedPrice for exact map key matching, double for quantities
using PriceLevel = std::pair<FixedPrice, Quantity>;

/// Depth update from @depth stream
struct DepthUpdate {
    std::string event_type;       // "depthUpdate"
    std::uint64_t event_time;     // Event time in ms
    std::uint64_t transaction_time;
    std::string symbol;
    SequenceId first_update_id;   // First update ID in event
    SequenceId final_update_id;   // Final update ID in event
    SequenceId prev_final_update_id;  // Previous final update ID (for sync)
    std::vector<PriceLevel> bids;
    std::vector<PriceLevel> asks;
};

/// Aggregated trade from @aggTrade stream
struct AggTrade {
    std::string event_type;       // "aggTrade"
    std::uint64_t event_time;     // Event time in ms
    std::string symbol;
    TradeId agg_trade_id;
    Price price;
    Quantity quantity;
    TradeId first_trade_id;
    TradeId last_trade_id;
    std::uint64_t trade_time;     // Trade time in ms
    bool is_buyer_maker;          // true = sell aggressor, false = buy aggressor
};

/// Depth snapshot from REST API
struct DepthSnapshot {
    SequenceId last_update_id;
    std::uint64_t event_time;     // Time received
    std::string symbol;
    std::vector<PriceLevel> bids;
    std::vector<PriceLevel> asks;
};

/// Combined stream wrapper (Binance sends stream name with data)
struct StreamMessage {
    std::string stream;           // e.g., "btcusdt@depth@100ms"
    std::string data;             // Raw JSON data
};

}  // namespace titan::binance
