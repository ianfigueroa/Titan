#pragma once

#include <chrono>
#include <cstdint>
#include <string>

namespace titan {

// Price in quote currency (e.g., USDT)
using Price = double;

// Quantity in base currency (e.g., BTC)
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

}  // namespace titan
