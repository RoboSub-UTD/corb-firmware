#include <gtest/gtest.h>

#include <cmath>
#include <limits>

#include "corb/command_gate.hpp"

using corb::CommandGate;
using corb::ThrusterArray;

namespace
{
constexpr float kOk[4] = {0.1f, -0.2f, 0.3f, -1.0f};
}

TEST(CommandGate, NothingStoredIsNeutral)
{
    CommandGate g;
    EXPECT_FALSE(g.fresh(0));
    for (float v : g.sample(0))
        EXPECT_EQ(v, 0.0f);
}

TEST(CommandGate, AcceptsAndSamplesFresh)
{
    CommandGate g;
    ASSERT_TRUE(g.submit(1, 100, kOk, 4));
    EXPECT_TRUE(g.fresh(100));
    const ThrusterArray s = g.sample(200);
    EXPECT_FLOAT_EQ(s[0], 0.1f);
    EXPECT_FLOAT_EQ(s[3], -1.0f);
    EXPECT_FLOAT_EQ(s[4], 0.0f);
}

TEST(CommandGate, StaleCommandIsZeroed)
{
    CommandGate g;
    ASSERT_TRUE(g.submit(1, 100, kOk, 4));
    EXPECT_TRUE(g.fresh(349));
    EXPECT_FALSE(g.fresh(350));
    for (float v : g.sample(350))
        EXPECT_EQ(v, 0.0f);
}

TEST(CommandGate, RejectsBadInputs)
{
    CommandGate g;
    EXPECT_FALSE(g.submit(1, 0, nullptr, 4));
    EXPECT_FALSE(g.submit(1, 0, kOk, 3));
    EXPECT_FALSE(g.submit(1, 0, kOk, 5));
    const float nan[4] = {0, 0, std::nanf(""), 0};
    const float inf[4] = {0, std::numeric_limits<float>::infinity(), 0, 0};
    const float big[4] = {0, 0, 0, 1.01f};
    EXPECT_FALSE(g.submit(1, 0, nan, 4));
    EXPECT_FALSE(g.submit(1, 0, inf, 4));
    EXPECT_FALSE(g.submit(1, 0, big, 4));
    EXPECT_FALSE(g.fresh(0));
}

TEST(CommandGate, RejectedDoesNotRefreshTimerOrReplaceCommand)
{
    CommandGate g;
    ASSERT_TRUE(g.submit(5, 100, kOk, 4));
    const float bad[4] = {0, 0, 0, 2.0f};
    EXPECT_FALSE(g.submit(6, 300, bad, 4));
    EXPECT_FALSE(g.submit(5, 300, kOk, 4)); // duplicate seq
    EXPECT_FALSE(g.fresh(350));
}

TEST(CommandGate, RejectsOldSequenceAcceptsNewer)
{
    CommandGate g;
    ASSERT_TRUE(g.submit(10, 0, kOk, 4));
    EXPECT_FALSE(g.submit(9, 1, kOk, 4));
    EXPECT_TRUE(g.submit(11, 2, kOk, 4));
}

TEST(CommandGate, SequenceWraparound)
{
    CommandGate g;
    ASSERT_TRUE(g.submit(0xFFFFFFFFu, 0, kOk, 4));
    EXPECT_TRUE(g.submit(0u, 1, kOk, 4));
    EXPECT_FALSE(g.submit(0xFFFFFFFFu, 2, kOk, 4));
}

TEST(CommandGate, TimeWraparound)
{
    CommandGate g;
    ASSERT_TRUE(g.submit(1, 0xFFFFFFF0u, kOk, 4));
    EXPECT_TRUE(g.fresh(0x10u));  // 32 ms later
    EXPECT_FALSE(g.fresh(0xF0u)); // 256 ms later
}

TEST(CommandGate, ResetDiscards)
{
    CommandGate g;
    ASSERT_TRUE(g.submit(1, 0, kOk, 4));
    g.reset();
    EXPECT_FALSE(g.fresh(0));
    EXPECT_TRUE(g.submit(1, 0, kOk, 4));
}

TEST(CommandGate, ThrusterCountClamped)
{
    CommandGate g({250, 100});
    float v[8] = {};
    EXPECT_TRUE(g.submit(1, 0, v, 8));
    EXPECT_FALSE(g.submit(2, 0, v, 4));
}
