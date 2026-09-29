#include <gtest/gtest.h>
#include "core/latency_stats.hpp"

using namespace titan;
using std::chrono::nanoseconds;

TEST(LatencyStatsTest, EmptySummaryIsZero) {
    LatencyStats stats(16);
    auto s = stats.summary();

    EXPECT_EQ(s.count, 0u);
    EXPECT_EQ(s.p50.count(), 0);
    EXPECT_EQ(s.p99.count(), 0);
    EXPECT_EQ(s.max.count(), 0);
}

TEST(LatencyStatsTest, NearestRankPercentiles) {
    LatencyStats stats(1000);
    for (int i = 100; i >= 1; --i) {
        stats.record(nanoseconds(i));
    }

    auto s = stats.summary();
    EXPECT_EQ(s.count, 100u);
    EXPECT_EQ(s.p50.count(), 50);
    EXPECT_EQ(s.p99.count(), 99);
    EXPECT_EQ(s.max.count(), 100);
}

TEST(LatencyStatsTest, SingleSample) {
    LatencyStats stats(8);
    stats.record(nanoseconds(42));

    auto s = stats.summary();
    EXPECT_EQ(s.count, 1u);
    EXPECT_EQ(s.p50.count(), 42);
    EXPECT_EQ(s.p99.count(), 42);
    EXPECT_EQ(s.max.count(), 42);
}

TEST(LatencyStatsTest, WindowKeepsMostRecentSamples) {
    LatencyStats stats(4);
    for (int i = 1; i <= 4; ++i) {
        stats.record(nanoseconds(1000));
    }
    for (int i = 1; i <= 4; ++i) {
        stats.record(nanoseconds(i));
    }

    auto s = stats.summary();
    EXPECT_EQ(s.count, 4u);
    EXPECT_EQ(s.max.count(), 4);
    EXPECT_EQ(s.p50.count(), 2);
}

TEST(LatencyStatsTest, ClearDropsSamples) {
    LatencyStats stats(8);
    stats.record(nanoseconds(5));
    stats.clear();

    EXPECT_EQ(stats.summary().count, 0u);
}

TEST(LatencyStatsTest, TotalCountsEverySampleEvenPastTheWindow) {
    LatencyStats stats(4);
    for (int i = 0; i < 10; ++i) {
        stats.record(nanoseconds(1));
    }

    EXPECT_EQ(stats.total(), 10u);
    EXPECT_EQ(stats.summary().count, 4u);
}
