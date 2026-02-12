#pragma once

#include "orderbook/snapshot.hpp"
#include "trade/alert_detector.hpp"
#include "trade/trade_flow.hpp"
#include <chrono>
#include <string>

namespace titan::output {

/// Console output for market data metrics
class ConsoleLogger {
public:
    /// Create a console logger
    /// @param interval Minimum time between metric outputs
    explicit ConsoleLogger(std::chrono::milliseconds interval);

    /// Log current metrics (respects rate limiting)
    /// @return true if logged, false if rate limited
    bool log_metrics(const BookSnapshot& book, const TradeFlowMetrics& flow);

    /// Log a trade alert (always logs, not rate limited)
    void log_alert(const TradeAlert& alert);

    /// Log connection status change
    void log_connection_status(bool connected, const std::string& details = "");

    /// Log sync status
    void log_sync_status(const std::string& status);

    /// Force next log_metrics to output regardless of rate limit
    void force_next();

private:
    std::chrono::milliseconds interval_;
    std::chrono::steady_clock::time_point last_output_;
    bool force_next_{true};  // Always log first one
};

}  // namespace titan::output
