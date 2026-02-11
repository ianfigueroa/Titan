#pragma once

#include "core/types.hpp"
#include <cstddef>
#include <deque>

namespace titan {

/// Rolling VWAP calculator with online statistics
class VwapCalculator {
public:
    /// Create a VWAP calculator with specified window size
    /// @param window_size Number of trades to include in rolling window
    explicit VwapCalculator(std::size_t window_size = 100);

    /// Add a trade and return the updated VWAP
    /// @param price Trade price
    /// @param quantity Trade quantity
    /// @return Updated VWAP value
    [[nodiscard]] double add_trade(Price price, Quantity quantity);

    /// Get current VWAP (0.0 if no trades)
    [[nodiscard]] double vwap() const noexcept;

    /// Get total volume in window
    [[nodiscard]] double total_volume() const noexcept;

    /// Get number of trades in window
    [[nodiscard]] std::size_t trade_count() const noexcept;

    /// Get rolling average trade size
    [[nodiscard]] double rolling_avg_size() const noexcept;

    /// Get rolling standard deviation of trade sizes
    [[nodiscard]] double rolling_std_dev() const noexcept;

    /// Clear all trades
    void clear();

private:
    struct Trade {
        Price price;
        Quantity quantity;
    };

    std::deque<Trade> trades_;
    std::size_t window_size_;

    // Running sums for VWAP calculation
    double sum_pv_{0.0};  // Sum of price * volume
    double sum_v_{0.0};   // Sum of volume

    // Welford's algorithm for online variance
    double mean_{0.0};
    double m2_{0.0};      // Sum of squared differences from mean
    std::size_t count_{0};
};

}  // namespace titan
