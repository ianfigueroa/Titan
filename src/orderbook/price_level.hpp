#pragma once

#include "core/types.hpp"
#include <functional>
#include <map>

namespace titan {

/// Bid side: prices sorted descending (highest first)
/// Uses FixedPrice as key for exact matching, double quantity for accumulation
using BidSide = std::map<FixedPrice, Quantity, std::greater<FixedPrice>>;

/// Ask side: prices sorted ascending (lowest first)
/// Uses FixedPrice as key for exact matching, double quantity for accumulation
using AskSide = std::map<FixedPrice, Quantity>;

/// Legacy double-keyed maps (for compatibility during migration)
namespace legacy {
using BidSide = std::map<Price, Quantity, std::greater<Price>>;
using AskSide = std::map<Price, Quantity>;
}  // namespace legacy

}  // namespace titan
