#pragma once

#include "binance/types.hpp"
#include "orderbook/price_level.hpp"
#include "orderbook/snapshot.hpp"
#include <cstddef>

namespace titan {

/// Local order book engine
/// Maintains bid/ask sides with cached best iterators for O(1) BBO access
class OrderBook {
public:
    /// Create an order book
    /// @param imbalance_levels Number of levels to use for imbalance calculation
    explicit OrderBook(std::size_t imbalance_levels = 5);

    /// Apply a full depth snapshot, replacing all existing data
    /// @return Current book metrics
    [[nodiscard]] BookSnapshot apply_snapshot(const binance::DepthSnapshot& snapshot);

    /// Apply an incremental depth update
    /// @return Current book metrics
    [[nodiscard]] BookSnapshot apply_update(const binance::DepthUpdate& update);

    /// Get current book state as immutable snapshot
    [[nodiscard]] BookSnapshot snapshot() const;

    /// Get last processed update ID
    [[nodiscard]] SequenceId last_update_id() const noexcept;

    /// Check if there's a sequence gap
    /// @param first_update_id First update ID in the new update
    /// @param prev_final_update_id Previous final update ID from the new update
    /// @return true if there's a gap (need to re-sync)
    [[nodiscard]] bool has_sequence_gap(SequenceId first_update_id,
                                        SequenceId prev_final_update_id) const noexcept;

    /// Clear all book data
    void clear();

    /// Get number of bid levels
    [[nodiscard]] std::size_t bid_levels() const noexcept;

    /// Get number of ask levels
    [[nodiscard]] std::size_t ask_levels() const noexcept;

private:
    void apply_bid_update(Price price, Quantity qty);
    void apply_ask_update(Price price, Quantity qty);
    void invalidate_best_cache();
    void update_best_bid_cache() const;
    void update_best_ask_cache() const;
    [[nodiscard]] double calculate_imbalance() const;
    [[nodiscard]] BookSnapshot build_snapshot() const;

    BidSide bids_;
    AskSide asks_;
    SequenceId last_update_id_{0};
    std::size_t imbalance_levels_;

    // Cached best iterators (mutable for const snapshot())
    mutable BidSide::const_iterator best_bid_it_;
    mutable AskSide::const_iterator best_ask_it_;
    mutable bool best_bid_valid_{false};
    mutable bool best_ask_valid_{false};
};

}  // namespace titan
