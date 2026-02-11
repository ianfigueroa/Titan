#pragma once

#include "core/types.hpp"
#include <chrono>
#include <optional>

namespace titan {

/// Large trade alert
struct TradeAlert {
    Price price;
    Quantity quantity;
    bool is_buy;
    double deviation;  // Number of standard deviations from mean
    Timestamp timestamp;
};

/// Detects large trades that exceed threshold
class AlertDetector {
public:
    /// Create an alert detector
    /// @param std_dev_threshold Number of standard deviations for alert (e.g., 2.0)
    explicit AlertDetector(double std_dev_threshold = 2.0);

    /// Check if a trade should trigger an alert
    /// @param price Trade price
    /// @param quantity Trade quantity
    /// @param is_buy True if buy aggressor
    /// @param rolling_avg Current rolling average trade size
    /// @param rolling_std_dev Current rolling standard deviation
    /// @return Alert if triggered, nullopt otherwise
    [[nodiscard]] std::optional<TradeAlert> check_trade(
        Price price,
        Quantity quantity,
        bool is_buy,
        double rolling_avg,
        double rolling_std_dev
    );

    /// Get the threshold
    [[nodiscard]] double threshold() const noexcept;

    /// Set a new threshold
    void set_threshold(double threshold);

private:
    double threshold_;
};

}  // namespace titan
