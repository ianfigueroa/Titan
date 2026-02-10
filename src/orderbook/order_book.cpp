#include "orderbook/order_book.hpp"
#include <algorithm>
#include <cmath>

namespace titan {

OrderBook::OrderBook(std::size_t imbalance_levels)
    : imbalance_levels_(imbalance_levels)
{}

BookSnapshot OrderBook::apply_snapshot(const binance::DepthSnapshot& snapshot) {
    // Clear existing data
    bids_.clear();
    asks_.clear();
    invalidate_best_cache();

    // Apply all levels from snapshot
    for (const auto& [price, qty] : snapshot.bids) {
        if (qty > 0.0) {
            bids_[price] = qty;
        }
    }

    for (const auto& [price, qty] : snapshot.asks) {
        if (qty > 0.0) {
            asks_[price] = qty;
        }
    }

    last_update_id_ = snapshot.last_update_id;

    return build_snapshot();
}

BookSnapshot OrderBook::apply_update(const binance::DepthUpdate& update) {
    // Apply bid updates
    for (const auto& [price, qty] : update.bids) {
        apply_bid_update(price, qty);
    }

    // Apply ask updates
    for (const auto& [price, qty] : update.asks) {
        apply_ask_update(price, qty);
    }

    last_update_id_ = update.final_update_id;

    return build_snapshot();
}

void OrderBook::apply_bid_update(Price price, Quantity qty) {
    if (qty > 0.0) {
        bids_[price] = qty;
    } else {
        auto it = bids_.find(price);
        if (it != bids_.end()) {
            // Check if we're removing the best bid
            if (best_bid_valid_ && it == best_bid_it_) {
                best_bid_valid_ = false;
            }
            bids_.erase(it);
        }
    }
    // Invalidate cache if a better price might have been added
    best_bid_valid_ = false;
}

void OrderBook::apply_ask_update(Price price, Quantity qty) {
    if (qty > 0.0) {
        asks_[price] = qty;
    } else {
        auto it = asks_.find(price);
        if (it != asks_.end()) {
            // Check if we're removing the best ask
            if (best_ask_valid_ && it == best_ask_it_) {
                best_ask_valid_ = false;
            }
            asks_.erase(it);
        }
    }
    // Invalidate cache if a better price might have been added
    best_ask_valid_ = false;
}

void OrderBook::invalidate_best_cache() {
    best_bid_valid_ = false;
    best_ask_valid_ = false;
}

void OrderBook::update_best_bid_cache() const {
    if (!best_bid_valid_) {
        best_bid_it_ = bids_.begin();
        best_bid_valid_ = true;
    }
}

void OrderBook::update_best_ask_cache() const {
    if (!best_ask_valid_) {
        best_ask_it_ = asks_.begin();
        best_ask_valid_ = true;
    }
}

double OrderBook::calculate_imbalance() const {
    if (bids_.empty() && asks_.empty()) {
        return 0.0;
    }

    Quantity bid_volume = 0.0;
    Quantity ask_volume = 0.0;

    // Sum top N levels
    std::size_t count = 0;
    for (auto it = bids_.begin(); it != bids_.end() && count < imbalance_levels_; ++it, ++count) {
        bid_volume += it->second;
    }

    count = 0;
    for (auto it = asks_.begin(); it != asks_.end() && count < imbalance_levels_; ++it, ++count) {
        ask_volume += it->second;
    }

    Quantity total = bid_volume + ask_volume;
    if (total <= 0.0) {
        return 0.0;
    }

    // Imbalance: positive = more bids, negative = more asks
    return (bid_volume - ask_volume) / total;
}

BookSnapshot OrderBook::build_snapshot() const {
    BookSnapshot snap;
    snap.last_update_id = last_update_id_;
    snap.timestamp = std::chrono::steady_clock::now();

    if (bids_.empty() || asks_.empty()) {
        // Return default values for empty book
        if (!bids_.empty()) {
            update_best_bid_cache();
            snap.best_bid = best_bid_it_->first;
            snap.best_bid_qty = best_bid_it_->second;
        }
        if (!asks_.empty()) {
            update_best_ask_cache();
            snap.best_ask = best_ask_it_->first;
            snap.best_ask_qty = best_ask_it_->second;
        }
        snap.imbalance = calculate_imbalance();
        return snap;
    }

    // Update cached iterators
    update_best_bid_cache();
    update_best_ask_cache();

    snap.best_bid = best_bid_it_->first;
    snap.best_bid_qty = best_bid_it_->second;
    snap.best_ask = best_ask_it_->first;
    snap.best_ask_qty = best_ask_it_->second;

    snap.spread = snap.best_ask - snap.best_bid;
    snap.mid_price = (snap.best_bid + snap.best_ask) / 2.0;

    // Spread in basis points: (spread / mid) * 10000
    if (snap.mid_price > 0.0) {
        snap.spread_bps = (snap.spread / snap.mid_price) * 10000.0;
    }

    snap.imbalance = calculate_imbalance();

    return snap;
}

BookSnapshot OrderBook::snapshot() const {
    return build_snapshot();
}

SequenceId OrderBook::last_update_id() const noexcept {
    return last_update_id_;
}

bool OrderBook::has_sequence_gap(SequenceId /*first_update_id*/,
                                  SequenceId prev_final_update_id) const noexcept {
    // A gap exists if the previous final update ID doesn't match our last update ID
    return prev_final_update_id != last_update_id_;
}

void OrderBook::clear() {
    bids_.clear();
    asks_.clear();
    last_update_id_ = 0;
    invalidate_best_cache();
}

std::size_t OrderBook::bid_levels() const noexcept {
    return bids_.size();
}

std::size_t OrderBook::ask_levels() const noexcept {
    return asks_.size();
}

}  // namespace titan
