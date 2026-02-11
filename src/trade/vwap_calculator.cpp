#include "trade/vwap_calculator.hpp"
#include <cmath>

namespace titan {

VwapCalculator::VwapCalculator(std::size_t window_size)
    : window_size_(window_size)
{}

double VwapCalculator::add_trade(Price price, Quantity quantity) {
    // Add new trade
    trades_.push_back({price, quantity});
    sum_pv_ += price * quantity;
    sum_v_ += quantity;

    // Update Welford's algorithm for online statistics
    ++count_;
    double delta = quantity - mean_;
    mean_ += delta / static_cast<double>(count_);
    double delta2 = quantity - mean_;
    m2_ += delta * delta2;

    // Remove oldest trade if window is full
    if (trades_.size() > window_size_) {
        const auto& old_trade = trades_.front();

        // Update VWAP sums
        sum_pv_ -= old_trade.price * old_trade.quantity;
        sum_v_ -= old_trade.quantity;

        // Update Welford's algorithm for removal
        // Reverse the addition: count--, mean adjustment, m2 adjustment
        double old_qty = old_trade.quantity;
        double old_delta = old_qty - mean_;
        --count_;

        if (count_ > 0) {
            mean_ = (mean_ * (static_cast<double>(count_) + 1.0) - old_qty) / static_cast<double>(count_);
            double old_delta2 = old_qty - mean_;
            m2_ -= old_delta * old_delta2;
            // Clamp m2_ to avoid negative values due to floating point errors
            if (m2_ < 0.0) m2_ = 0.0;
        } else {
            mean_ = 0.0;
            m2_ = 0.0;
        }

        trades_.pop_front();
    }

    return vwap();
}

double VwapCalculator::vwap() const noexcept {
    if (sum_v_ <= 0.0) {
        return 0.0;
    }
    return sum_pv_ / sum_v_;
}

double VwapCalculator::total_volume() const noexcept {
    return sum_v_;
}

std::size_t VwapCalculator::trade_count() const noexcept {
    return trades_.size();
}

double VwapCalculator::rolling_avg_size() const noexcept {
    if (count_ == 0) {
        return 0.0;
    }
    return mean_;
}

double VwapCalculator::rolling_std_dev() const noexcept {
    if (count_ < 2) {
        return 0.0;
    }
    double variance = m2_ / static_cast<double>(count_);
    return std::sqrt(variance);
}

void VwapCalculator::clear() {
    trades_.clear();
    sum_pv_ = 0.0;
    sum_v_ = 0.0;
    mean_ = 0.0;
    m2_ = 0.0;
    count_ = 0;
}

}  // namespace titan
