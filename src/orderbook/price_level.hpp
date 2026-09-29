#pragma once

#include "core/types.hpp"
#include <algorithm>
#include <functional>
#include <utility>
#include <vector>

namespace titan {

/// Sorted vector, best price at the back
template <typename Worse>
class BookSide {
public:
    using Level = std::pair<FixedPrice, Quantity>;
    using const_reverse_iterator = typename std::vector<Level>::const_reverse_iterator;

    void set(FixedPrice price, Quantity qty) {
        auto it = std::lower_bound(levels_.begin(), levels_.end(), price,
                                   [](const Level& level, FixedPrice p) {
                                       return Worse{}(level.first, p);
                                   });
        bool found = it != levels_.end() && it->first == price;
        if (qty > 0.0) {
            if (found) {
                it->second = qty;
            } else {
                levels_.insert(it, Level{price, qty});
            }
        } else if (found) {
            levels_.erase(it);
        }
    }

    template <typename Levels>
    void assign(const Levels& levels) {
        levels_.clear();
        levels_.reserve(levels.size());
        for (const auto& [price, qty] : levels) {
            if (qty > 0.0) {
                levels_.emplace_back(price, qty);
            }
        }
        std::sort(levels_.begin(), levels_.end(), [](const Level& a, const Level& b) {
            return Worse{}(a.first, b.first);
        });
    }

    [[nodiscard]] const Level& best() const { return levels_.back(); }
    [[nodiscard]] const_reverse_iterator best_first() const { return levels_.crbegin(); }
    [[nodiscard]] const_reverse_iterator best_first_end() const { return levels_.crend(); }

    [[nodiscard]] bool empty() const noexcept { return levels_.empty(); }
    [[nodiscard]] std::size_t size() const noexcept { return levels_.size(); }
    void clear() noexcept { levels_.clear(); }

private:
    std::vector<Level> levels_;
};

/// Bids: highest price is best
using BidSide = BookSide<std::less<FixedPrice>>;

/// Asks: lowest price is best
using AskSide = BookSide<std::greater<FixedPrice>>;

}  // namespace titan
