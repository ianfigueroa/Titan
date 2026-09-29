#include "orderbook/order_book.hpp"
#include <algorithm>
#include <cmath>

namespace titan {

OrderBook::OrderBook(std::size_t imbalance_levels)
    : imbalance_levels_(imbalance_levels)
{}

BookSnapshot OrderBook::apply_snapshot(const binance::DepthSnapshot& snapshot) {
    bids_.assign(snapshot.bids);
    asks_.assign(snapshot.asks);
    last_update_id_ = snapshot.last_update_id;

    return build_snapshot();
}

BookSnapshot OrderBook::apply_update(const binance::DepthUpdate& update) {
    for (const auto& [price, qty] : update.bids) {
        bids_.set(price, qty);
    }

    for (const auto& [price, qty] : update.asks) {
        asks_.set(price, qty);
    }

    last_update_id_ = update.final_update_id;

    return build_snapshot();
}

double OrderBook::calculate_imbalance() const {
    if (bids_.empty() && asks_.empty()) {
        return 0.0;
    }

    Quantity bid_volume = 0.0;
    Quantity ask_volume = 0.0;

    // Sum top N levels
    std::size_t count = 0;
    for (auto it = bids_.best_first(); it != bids_.best_first_end() && count < imbalance_levels_; ++it, ++count) {
        bid_volume += it->second;
    }

    count = 0;
    for (auto it = asks_.best_first(); it != asks_.best_first_end() && count < imbalance_levels_; ++it, ++count) {
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

    if (!bids_.empty()) {
        snap.best_bid = bids_.best().first.to_double();
        snap.best_bid_qty = bids_.best().second;
    }
    if (!asks_.empty()) {
        snap.best_ask = asks_.best().first.to_double();
        snap.best_ask_qty = asks_.best().second;
    }

    if (!bids_.empty() && !asks_.empty()) {
        snap.spread = snap.best_ask - snap.best_bid;
        snap.mid_price = (snap.best_bid + snap.best_ask) / 2.0;

        // Spread in basis points: (spread / mid) * 10000
        if (snap.mid_price > 0.0) {
            snap.spread_bps = (snap.spread / snap.mid_price) * 10000.0;
        }
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
}

std::size_t OrderBook::bid_levels() const noexcept {
    return bids_.size();
}

std::size_t OrderBook::ask_levels() const noexcept {
    return asks_.size();
}

}  // namespace titan
