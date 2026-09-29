#include <gtest/gtest.h>
#include "binance/depth_sequencer.hpp"

using namespace titan;
using namespace titan::binance;
using Verdict = DepthSequencer::Verdict;

namespace {

DepthUpdate update(SequenceId first, SequenceId last, SequenceId prev) {
    DepthUpdate u{};
    u.first_update_id = first;
    u.final_update_id = last;
    u.prev_final_update_id = prev;
    return u;
}

}  // namespace

TEST(DepthSequencerTest, DropsUpdatesOlderThanSnapshot) {
    DepthSequencer seq;
    seq.reset(1000);

    EXPECT_EQ(seq.check(update(900, 950, 899)), Verdict::Stale);
    EXPECT_EQ(seq.check(update(951, 999, 950)), Verdict::Stale);
    EXPECT_FALSE(seq.bridged());
}

TEST(DepthSequencerTest, FirstUpdateMustStraddleSnapshot) {
    DepthSequencer seq;
    seq.reset(1000);

    EXPECT_EQ(seq.check(update(990, 1010, 989)), Verdict::Apply);
    EXPECT_TRUE(seq.bridged());
    EXPECT_EQ(seq.last_id(), 1010u);
}

TEST(DepthSequencerTest, BridgeCanEndExactlyAtSnapshotId) {
    DepthSequencer seq;
    seq.reset(1000);

    EXPECT_EQ(seq.check(update(990, 1000, 989)), Verdict::Apply);
    EXPECT_EQ(seq.check(update(1037, 1052, 1000)), Verdict::Apply);
}

TEST(DepthSequencerTest, UpdateStartingAfterSnapshotIsAGap) {
    DepthSequencer seq;
    seq.reset(1000);

    EXPECT_EQ(seq.check(update(1001, 1020, 999)), Verdict::Gap);
}

TEST(DepthSequencerTest, FollowsPreviousFinalIdChain) {
    DepthSequencer seq;
    seq.reset(1000);

    EXPECT_EQ(seq.check(update(995, 1010, 994)), Verdict::Apply);
    EXPECT_EQ(seq.check(update(1040, 1060, 1010)), Verdict::Apply);
    EXPECT_EQ(seq.check(update(1061, 1075, 1060)), Verdict::Apply);
    EXPECT_EQ(seq.last_id(), 1075u);
}

TEST(DepthSequencerTest, BrokenChainIsAGapFromTheFirstLiveUpdate) {
    DepthSequencer seq;
    seq.reset(1000);

    EXPECT_EQ(seq.check(update(995, 1010, 994)), Verdict::Apply);
    EXPECT_EQ(seq.check(update(1040, 1060, 1030)), Verdict::Gap);
}

TEST(DepthSequencerTest, ResetStartsOverAfterAGap) {
    DepthSequencer seq;
    seq.reset(1000);
    EXPECT_EQ(seq.check(update(995, 1010, 994)), Verdict::Apply);
    EXPECT_EQ(seq.check(update(1040, 1060, 1030)), Verdict::Gap);

    seq.reset(2000);
    EXPECT_FALSE(seq.bridged());
    EXPECT_EQ(seq.check(update(1990, 2005, 1989)), Verdict::Apply);
}
