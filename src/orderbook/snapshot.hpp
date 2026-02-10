#pragma once

#include "core/types.hpp"
#include <chrono>

namespace titan {

/// Immutable snapshot of order book state
struct BookSnapshot {
    Price best_bid{0.0};
    Price best_ask{0.0};
    Quantity best_bid_qty{0.0};
    Quantity best_ask_qty{0.0};
    Price spread{0.0};
    BasisPoints spread_bps{0.0};
    Price mid_price{0.0};
    Percentage imbalance{0.0};  // -1.0 to 1.0, positive = more bids
    SequenceId last_update_id{0};
    Timestamp timestamp{};

    /// Check if book has valid data
    [[nodiscard]] bool is_valid() const noexcept {
        return best_bid > 0.0 && best_ask > 0.0 && best_ask > best_bid;
    }
};

}  // namespace titan
