#pragma once

#include "orderbook/snapshot.hpp"
#include "trade/alert_detector.hpp"
#include "trade/trade_flow.hpp"
#include <nlohmann/json.hpp>
#include <string>

namespace titan::output {

/// Formats data as JSON for WebSocket output
class JsonFormatter {
public:
    /// Format book and trade metrics as JSON
    [[nodiscard]] static nlohmann::json format_metrics(
        const BookSnapshot& book,
        const TradeFlowMetrics& flow
    );

    /// Format a trade alert as JSON
    [[nodiscard]] static nlohmann::json format_alert(const TradeAlert& alert);

    /// Format connection status as JSON
    [[nodiscard]] static nlohmann::json format_status(
        bool connected,
        const std::string& state
    );

    /// Get current ISO8601 timestamp string
    [[nodiscard]] static std::string iso_timestamp();
};

}  // namespace titan::output
