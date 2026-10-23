#include <gtest/gtest.h>

#include "corb/vcu.hpp"

using namespace corb;

namespace
{

class FakeThrusters final : public ThrusterOutput
{
public:
    void write(const ThrusterArray& v, std::size_t) override { last = v; }
    ThrusterArray last{};
};

class FakeKill final : public KillLine
{
public:
    void set_run(bool run) override { level = run; }
    bool level = false;
};

constexpr float kCmd[4] = {0.5f, 0.5f, -0.5f, -0.5f};

struct Fixture : ::testing::Test
{
    FakeThrusters thr;
    FakeKill kill;
    Vcu vcu{thr, kill, Vcu::Config{}};
    std::uint32_t now = 0;

    void SetUp() override { vcu.set_health({true, true}); }

    void step(std::uint32_t dt = 10)
    {
        now += dt;
        vcu.tick(now);
    }

    void arm()
    {
        vcu.handle(Event::BootDone);
        vcu.handle(Event::CheckPassed);
        vcu.handle(Event::ArmRequest);
        step();
    }

    void arm_guided()
    {
        arm();
        vcu.handle(Event::GuidedRequest);
    }

    bool outputs_neutral() const
    {
        for (float v : thr.last)
        {
            if (v != 0.0f)
                return false;
        }
        return true;
    }
};

} // namespace

TEST_F(Fixture, ArmPassesThroughPreArmAndRaisesKillLine)
{
    vcu.handle(Event::BootDone);
    vcu.handle(Event::CheckPassed);
    vcu.handle(Event::ArmRequest);
    EXPECT_EQ(vcu.state(), State::PreArm);
    EXPECT_FALSE(kill.level);
    step();
    EXPECT_EQ(vcu.state(), State::Armed);
    EXPECT_TRUE(kill.level);
}

TEST_F(Fixture, PreArmFailsOnTimeoutWhenRcLinkMissing)
{
    vcu.set_health({false, true});
    vcu.handle(Event::BootDone);
    vcu.handle(Event::CheckPassed);
    vcu.handle(Event::ArmRequest);
    step(500); // first tick: timer starts
    EXPECT_EQ(vcu.state(), State::PreArm);
    step(900);
    EXPECT_EQ(vcu.state(), State::PreArm);
    step(200);
    EXPECT_EQ(vcu.state(), State::Disarmed);
    EXPECT_FALSE(kill.level);
}

TEST_F(Fixture, PreArmPassesIfLinkArrivesInTime)
{
    vcu.set_health({false, true});
    vcu.handle(Event::BootDone);
    vcu.handle(Event::CheckPassed);
    vcu.handle(Event::ArmRequest);
    step(500);
    vcu.set_health({true, true});
    step();
    EXPECT_EQ(vcu.state(), State::Armed);
}

TEST_F(Fixture, GuidedOutputsFreshCommand)
{
    arm_guided();
    ASSERT_TRUE(vcu.submit_command(1, now, kCmd, 4));
    step();
    EXPECT_FLOAT_EQ(thr.last[0], 0.5f);
}

TEST_F(Fixture, StaleGuidedCommandFallsBackToRcAndNeutral)
{
    arm_guided();
    ASSERT_TRUE(vcu.submit_command(1, now, kCmd, 4));
    step();
    EXPECT_EQ(vcu.mode(), ControlMode::Guided);
    step(1000); // command now stale
    EXPECT_EQ(vcu.mode(), ControlMode::Rc);
    EXPECT_EQ(vcu.state(), State::Armed);
    EXPECT_TRUE(outputs_neutral());
}

TEST_F(Fixture, GuidedBeforeFirstCommandDoesNotFallBack)
{
    arm_guided();
    step(1000);
    EXPECT_EQ(vcu.mode(), ControlMode::Guided);
    EXPECT_TRUE(outputs_neutral());
}

TEST_F(Fixture, NotGuidedOutputsNeutralEvenWithFreshCommand)
{
    arm();
    ASSERT_TRUE(vcu.submit_command(1, now, kCmd, 4));
    step();
    EXPECT_TRUE(outputs_neutral());
}

TEST_F(Fixture, RcLinkLossDisarmsAfterTimeout)
{
    arm();
    vcu.set_health({false, true});
    step(10); // first tick: timer starts
    step(1900);
    EXPECT_EQ(vcu.state(), State::Armed);
    step(200);
    EXPECT_EQ(vcu.state(), State::Disarmed);
    EXPECT_FALSE(kill.level);
}

TEST_F(Fixture, RcLinkLossIgnoredWhileGuided)
{
    arm_guided();
    vcu.set_health({false, true});
    step(5000);
    EXPECT_EQ(vcu.state(), State::Armed);
}

TEST_F(Fixture, LowBatteryEntersRecoverableFaultAndClears)
{
    arm_guided();
    vcu.set_health({true, false});
    step();
    EXPECT_EQ(vcu.state(), State::RecoverableFault);
    EXPECT_FALSE(kill.level);
    EXPECT_TRUE(outputs_neutral());
    vcu.set_health({true, true});
    step();
    EXPECT_EQ(vcu.state(), State::Disarmed); // must re-arm explicitly
    EXPECT_FALSE(kill.level);
}

TEST_F(Fixture, KillZeroesOutputsAndDropsKillLine)
{
    arm_guided();
    ASSERT_TRUE(vcu.submit_command(1, now, kCmd, 4));
    step();
    vcu.assert_kill(KillSource::Software);
    EXPECT_FALSE(kill.level);
    EXPECT_EQ(vcu.state(), State::Killed);
    EXPECT_TRUE(outputs_neutral());
    step();
    EXPECT_TRUE(outputs_neutral());
}

TEST_F(Fixture, EachKillSourceReleasesOnlyItself)
{
    arm();
    vcu.assert_kill(KillSource::Software);
    vcu.assert_kill(KillSource::Wireless);
    vcu.release_kill(KillSource::Software);
    EXPECT_EQ(vcu.state(), State::Killed);
    EXPECT_EQ(vcu.kill_sources(), static_cast<std::uint8_t>(KillSource::Wireless));
    vcu.release_kill(KillSource::Wireless);
    EXPECT_EQ(vcu.state(), State::Recovering);
    EXPECT_EQ(vcu.kill_sources(), 0);
}
