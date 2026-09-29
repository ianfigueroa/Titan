#pragma once

#include "binance/types.hpp"
#include "core/status.hpp"
#include <string>
#include <string_view>
#include <variant>

namespace titan::binance {

using StreamEvent = std::variant<std::monostate, DepthUpdate, AggTrade>;

/// Parser for Binance WebSocket and REST API messages
/// Converts raw JSON to typed structs
class MessageParser {
public:
    /// Parse a depth update message from JSON
    [[nodiscard]] static Result<DepthUpdate, std::string>
    parse_depth_update(std::string_view json);

    /// Parse an aggregated trade message from JSON
    [[nodiscard]] static Result<AggTrade, std::string>
    parse_agg_trade(std::string_view json);

    /// Parse a depth snapshot from REST API response
    [[nodiscard]] static Result<DepthSnapshot, std::string>
    parse_depth_snapshot(std::string_view json, std::string_view symbol);

    /// Parse a combined stream message and its payload in one pass
    [[nodiscard]] static Result<StreamEvent, std::string>
    parse_stream_event(std::string_view json);

    /// Parse a combined stream wrapper message
    [[nodiscard]] static Result<StreamMessage, std::string>
    parse_combined_stream(std::string_view json);

    /// Check if stream name is a depth stream
    [[nodiscard]] static bool is_depth_stream(std::string_view stream_name);

    /// Check if stream name is an aggTrade stream
    [[nodiscard]] static bool is_agg_trade_stream(std::string_view stream_name);
};

}  // namespace titan::binance
