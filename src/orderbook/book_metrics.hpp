#pragma once

#include "orderbook/snapshot.hpp"

namespace titan {

/// Additional order book metrics calculations
namespace book_metrics {

/// Calculate volume-weighted mid price
/// @param book Current book snapshot
/// @return VWAP mid price, or regular mid if quantities are zero
[[nodiscard]] inline Price vwap_mid(const BookSnapshot& book) {
    Quantity total_qty = book.best_bid_qty + book.best_ask_qty;
    if (total_qty <= 0.0) {
        return book.mid_price;
    }

    return (book.best_bid * book.best_ask_qty + book.best_ask * book.best_bid_qty) / total_qty;
}

/// Calculate micro price (inventory-adjusted mid)
/// @param book Current book snapshot
/// @return Micro price giving more weight to side with less quantity
[[nodiscard]] inline Price micro_price(const BookSnapshot& book) {
    Quantity total_qty = book.best_bid_qty + book.best_ask_qty;
    if (total_qty <= 0.0) {
        return book.mid_price;
    }

    // Weight towards side with less quantity (more likely to be hit)
    double bid_weight = book.best_ask_qty / total_qty;
    double ask_weight = book.best_bid_qty / total_qty;

    return book.best_bid * bid_weight + book.best_ask * ask_weight;
}

}  // namespace book_metrics

}  // namespace titan
