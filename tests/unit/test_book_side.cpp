#include <gtest/gtest.h>
#include "orderbook/price_level.hpp"
#include <vector>

using namespace titan;

namespace {

template <typename Side>
std::vector<double> prices_best_first(const Side& side) {
    std::vector<double> out;
    for (auto it = side.best_first(); it != side.best_first_end(); ++it) {
        out.push_back(it->first.to_double());
    }
    return out;
}

}  // namespace

TEST(BookSideTest, BidsKeepHighestPriceBest) {
    BidSide bids;
    bids.set(FixedPrice(100.0), 1.0);
    bids.set(FixedPrice(102.0), 2.0);
    bids.set(FixedPrice(101.0), 3.0);

    ASSERT_EQ(bids.size(), 3u);
    EXPECT_DOUBLE_EQ(bids.best().first.to_double(), 102.0);
    EXPECT_EQ(prices_best_first(bids), (std::vector<double>{102.0, 101.0, 100.0}));
}

TEST(BookSideTest, AsksKeepLowestPriceBest) {
    AskSide asks;
    asks.set(FixedPrice(101.0), 1.0);
    asks.set(FixedPrice(100.5), 2.0);
    asks.set(FixedPrice(103.0), 3.0);

    EXPECT_DOUBLE_EQ(asks.best().first.to_double(), 100.5);
    EXPECT_EQ(prices_best_first(asks), (std::vector<double>{100.5, 101.0, 103.0}));
}

TEST(BookSideTest, SetUpdatesExistingLevel) {
    BidSide bids;
    bids.set(FixedPrice(100.0), 1.0);
    bids.set(FixedPrice(100.0), 4.5);

    ASSERT_EQ(bids.size(), 1u);
    EXPECT_DOUBLE_EQ(bids.best().second, 4.5);
}

TEST(BookSideTest, ZeroQuantityRemovesLevel) {
    AskSide asks;
    asks.set(FixedPrice(100.0), 1.0);
    asks.set(FixedPrice(101.0), 1.0);
    asks.set(FixedPrice(100.0), 0.0);

    ASSERT_EQ(asks.size(), 1u);
    EXPECT_DOUBLE_EQ(asks.best().first.to_double(), 101.0);
}

TEST(BookSideTest, RemovingMissingLevelIsANoOp) {
    BidSide bids;
    bids.set(FixedPrice(100.0), 1.0);
    bids.set(FixedPrice(99.0), 0.0);

    EXPECT_EQ(bids.size(), 1u);
}

TEST(BookSideTest, AssignSortsUnorderedLevelsAndDropsZeros) {
    BidSide bids;
    std::vector<std::pair<FixedPrice, Quantity>> levels = {
        {FixedPrice(100.0), 1.0}, {FixedPrice(103.0), 2.0},
        {FixedPrice(101.0), 0.0}, {FixedPrice(102.0), 3.0}};
    bids.assign(levels);

    EXPECT_EQ(prices_best_first(bids), (std::vector<double>{103.0, 102.0, 100.0}));
}

TEST(BookSideTest, ClearEmptiesSide) {
    AskSide asks;
    asks.set(FixedPrice(100.0), 1.0);
    asks.clear();

    EXPECT_TRUE(asks.empty());
}
