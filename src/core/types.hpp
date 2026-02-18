#pragma once

#include "core/fixed_point.hpp"
#include <chrono>
#include <cstdint>
#include <string>

namespace titan {

// Fixed-point types for financial precision
// 8 decimal places matches Binance's precision for most pairs
constexpr int kPriceDecimals = 8;
constexpr int kQuantityDecimals = 8;

// Price in quote currency (e.g., USDT) - fixed-point for precision
using FixedPrice = FixedPoint<kPriceDecimals>;

// Quantity in base currency (e.g., BTC) - fixed-point for precision
using FixedQuantity = FixedPoint<kQuantityDecimals>;

// Legacy double types for gradual migration
// These will be deprecated in favor of Fixed* types
using Price = double;
using Quantity = double;

// High-resolution timestamp for internal tracking
using Timestamp = std::chrono::steady_clock::time_point;

// Wall clock time for external display
using WallTime = std::chrono::system_clock::time_point;

// Binance sequence IDs
using SequenceId = std::uint64_t;

// Trade identifiers
using TradeId = std::uint64_t;

// Symbol identifier
using Symbol = std::string;

// Basis points (1 bp = 0.01%)
using BasisPoints = double;

// Percentage (0.0 to 1.0)
using Percentage = double;

// Conversion utilities
namespace convert {

/// Convert double price to fixed-point
[[nodiscard]] inline FixedPrice to_fixed_price(double d) {
    return FixedPrice(d);
}

/// Convert double quantity to fixed-point
[[nodiscard]] inline FixedQuantity to_fixed_quantity(double d) {
    return FixedQuantity(d);
}

/// Convert fixed-point to double (for display/logging)
/// Works for any FixedPoint type
template <int Decimals>
[[nodiscard]] inline double to_double(FixedPoint<Decimals> fp) {
    return fp.to_double();
}

/// Parse price from string
[[nodiscard]] inline FixedPrice parse_price(const std::string& s) {
    return FixedPrice::parse(s);
}

/// Parse quantity from string
[[nodiscard]] inline FixedQuantity parse_quantity(const std::string& s) {
    return FixedQuantity::parse(s);
}

}  // namespace convert

}  // namespace titan
