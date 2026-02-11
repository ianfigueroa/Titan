#pragma once

#include "binance/types.hpp"
#include "core/config.hpp"
#include "trade/alert_detector.hpp"
#include "trade/vwap_calculator.hpp"
#include <optional>

namespace titan {

/// Aggregated trade flow metrics
struct TradeFlowMetrics {
    double vwap{0.0};
    Quantity total_buy_volume{0.0};
    Quantity total_sell_volume{0.0};
    double net_flow{0.0};  // buy - sell
    std::size_t trade_count{0};
    std::optional<TradeAlert> last_alert;
};

/// Trade flow aggregator
/// Processes trades and maintains VWAP, volume metrics, and alert detection
class TradeFlow {
public:
    /// Create a trade flow aggregator
    /// @param config Engine configuration
    explicit TradeFlow(const Config::Engine& config);

    /// Process a trade and return updated metrics
    /// @param trade Aggregated trade from Binance
    /// @return Updated flow metrics
    [[nodiscard]] TradeFlowMetrics process_trade(const binance::AggTrade& trade);

    /// Get current metrics without processing a new trade
    [[nodiscard]] TradeFlowMetrics current_metrics() const;

    /// Reset all metrics
    void reset();

private:
    VwapCalculator vwap_;
    AlertDetector alert_detector_;

    Quantity total_buy_volume_{0.0};
    Quantity total_sell_volume_{0.0};
    std::optional<TradeAlert> last_alert_;
};

}  // namespace titan
