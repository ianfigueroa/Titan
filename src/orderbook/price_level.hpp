#pragma once

#include "core/types.hpp"
#include <functional>
#include <map>

namespace titan {

/// Bid side: prices sorted descending (highest first)
using BidSide = std::map<Price, Quantity, std::greater<Price>>;

/// Ask side: prices sorted ascending (lowest first)
using AskSide = std::map<Price, Quantity>;

}  // namespace titan
