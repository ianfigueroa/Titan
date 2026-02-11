#include "trade/alert_detector.hpp"
#include <cmath>

namespace titan {

AlertDetector::AlertDetector(double std_dev_threshold)
    : threshold_(std_dev_threshold)
{}

std::optional<TradeAlert> AlertDetector::check_trade(
    Price price,
    Quantity quantity,
    bool is_buy,
    double rolling_avg,
    double rolling_std_dev
) {
    // Guard against division by zero
    if (rolling_std_dev <= 0.0) {
        return std::nullopt;
    }

    double deviation = (quantity - rolling_avg) / rolling_std_dev;

    // Only alert on positive deviations (larger than normal)
    if (deviation > threshold_) {
        return TradeAlert{
            .price = price,
            .quantity = quantity,
            .is_buy = is_buy,
            .deviation = deviation,
            .timestamp = std::chrono::steady_clock::now()
        };
    }

    return std::nullopt;
}

double AlertDetector::threshold() const noexcept {
    return threshold_;
}

void AlertDetector::set_threshold(double threshold) {
    threshold_ = threshold;
}

}  // namespace titan
