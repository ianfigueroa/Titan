#pragma once

#include "core/types.hpp"
#include "binance/types.hpp"
#include <chrono>
#include <string>
#include <variant>

namespace titan {

/// Depth update message with receive timestamp
struct DepthUpdateMsg {
    binance::DepthUpdate data;
    Timestamp received_at;
};

/// Aggregated trade message with receive timestamp
struct AggTradeMsg {
    binance::AggTrade data;
    Timestamp received_at;
};

/// Snapshot received from REST API
struct SnapshotMsg {
    binance::DepthSnapshot data;
    Timestamp received_at;
};

/// Connection lost event
struct ConnectionLost {
    std::string reason;
    Timestamp occurred_at;
};

/// Connection restored event
struct ConnectionRestored {
    Timestamp occurred_at;
};

/// Sequence gap detected - need to re-sync
struct SequenceGap {
    SequenceId expected;
    SequenceId received;
    Timestamp detected_at;
};

/// Shutdown request
struct Shutdown {};

/// Unified message type for queue communication
using EngineMessage = std::variant<
    DepthUpdateMsg,
    AggTradeMsg,
    SnapshotMsg,
    ConnectionLost,
    ConnectionRestored,
    SequenceGap,
    Shutdown
>;

/// Helper to get message type name for logging
[[nodiscard]] inline std::string_view message_type_name(const EngineMessage& msg) {
    return std::visit([](const auto& m) -> std::string_view {
        using T = std::decay_t<decltype(m)>;
        if constexpr (std::is_same_v<T, DepthUpdateMsg>) return "DepthUpdate";
        else if constexpr (std::is_same_v<T, AggTradeMsg>) return "AggTrade";
        else if constexpr (std::is_same_v<T, SnapshotMsg>) return "Snapshot";
        else if constexpr (std::is_same_v<T, ConnectionLost>) return "ConnectionLost";
        else if constexpr (std::is_same_v<T, ConnectionRestored>) return "ConnectionRestored";
        else if constexpr (std::is_same_v<T, SequenceGap>) return "SequenceGap";
        else if constexpr (std::is_same_v<T, Shutdown>) return "Shutdown";
        else return "Unknown";
    }, msg);
}

}  // namespace titan
