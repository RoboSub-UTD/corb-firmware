#include <gtest/gtest.h>

#include <cstdint>

#include "corb/state_machine.hpp"

using namespace corb;

namespace
{
constexpr int kStates = static_cast<int>(kStateCount);
constexpr int kEvents = static_cast<int>(kEventCount);
} // namespace

TEST(StateMachine, HandleUpdatesOwnedState)
{
    StateMachine sm;
    EXPECT_EQ(sm.state(), State::Boot);
    EXPECT_EQ(sm.handle(Event::BootDone).next, State::SelfCheck);
    EXPECT_EQ(sm.state(), State::SelfCheck);
    sm.handle(Event::KillAsserted);
    EXPECT_EQ(sm.state(), State::Killed);
}

TEST(StateMachine, HappyPath)
{
    State s = State::Boot;
    for (Event e : {Event::BootDone, Event::CheckPassed, Event::ArmRequest, Event::PreArmPassed})
    {
        s = StateMachine::next(s, e).next;
    }
    EXPECT_EQ(s, State::Armed);
    EXPECT_EQ(StateMachine::next(s, Event::DisarmRequest).next, State::Disarmed);
}

TEST(StateMachine, ArmRaisesKillLine)
{
    // Requesting arm does not raise the kill line; passing the pre-arm checks does.
    EXPECT_FALSE(StateMachine::next(State::Disarmed, Event::ArmRequest).actions &
                 Action::KillLineHigh);
    EXPECT_EQ(StateMachine::next(State::Disarmed, Event::ArmRequest).next, State::PreArm);
    const Transition t = StateMachine::next(State::PreArm, Event::PreArmPassed);
    EXPECT_EQ(t.next, State::Armed);
    EXPECT_TRUE(t.actions & Action::KillLineHigh);
}

TEST(StateMachine, SelfCheckFailureFaults)
{
    EXPECT_EQ(StateMachine::next(State::SelfCheck, Event::CheckFailed).next, State::Fault);
}

TEST(StateMachine, KillAlwaysWinsAndIsSafe)
{
    for (int s = 0; s < kStates; ++s)
    {
        const auto from = static_cast<State>(s);
        if (from == State::Fault)
            continue;
        const Transition t = StateMachine::next(from, Event::KillAsserted);
        EXPECT_EQ(t.next, State::Killed) << to_string(from);
        EXPECT_TRUE(t.actions & Action::NeutralOutputs);
        EXPECT_TRUE(t.actions & Action::KillLineLow);
        EXPECT_FALSE(t.actions & Action::KillLineHigh);
    }
}

TEST(StateMachine, KillRecoveryPath)
{
    State s = StateMachine::next(State::Armed, Event::KillAsserted).next;
    EXPECT_EQ(s, State::Killed);
    // Arming while killed is ignored.
    EXPECT_EQ(StateMachine::next(s, Event::ArmRequest).next, State::Killed);
    s = StateMachine::next(s, Event::KillReleased).next;
    EXPECT_EQ(s, State::Recovering);
    EXPECT_EQ(StateMachine::next(s, Event::ArmRequest).next, State::Recovering);
    s = StateMachine::next(s, Event::SettleElapsed).next;
    EXPECT_EQ(s, State::Disarmed); // must re-arm explicitly
}

TEST(StateMachine, FaultIsTerminal)
{
    for (int e = 0; e < kEvents; ++e)
    {
        const Transition t = StateMachine::next(State::Fault, static_cast<Event>(e));
        EXPECT_EQ(t.next, State::Fault);
        EXPECT_TRUE(t.actions & Action::KillLineLow);
        EXPECT_FALSE(t.actions & Action::KillLineHigh);
    }
}

TEST(StateMachine, FaultDetectedFromAnywhere)
{
    for (int s = 0; s < kStates; ++s)
    {
        EXPECT_EQ(StateMachine::next(static_cast<State>(s), Event::FaultDetected).next,
                  State::Fault);
    }
}

TEST(StateMachine, TableIsTotalAndNeverRaisesKillLineOutsideArming)
{
    for (int s = 0; s < kStates; ++s)
    {
        for (int e = 0; e < kEvents; ++e)
        {
            const auto from = static_cast<State>(s);
            const auto ev = static_cast<Event>(e);
            const Transition t = StateMachine::next(from, ev);
            EXPECT_LT(static_cast<int>(t.next), kStates);
            if (t.actions & Action::KillLineHigh)
            {
                EXPECT_EQ(from, State::PreArm);
                EXPECT_EQ(ev, Event::PreArmPassed);
            }
            EXPECT_FALSE((t.actions & Action::KillLineHigh) && (t.actions & Action::KillLineLow));
        }
    }
}

TEST(StateMachine, Names)
{
    EXPECT_STREQ(to_string(State::Armed), "Armed");
    EXPECT_STREQ(to_string(Event::KillReleased), "KillReleased");
}

TEST(StateMachine, ModeOnlyChangesWhileArmed)
{
    StateMachine sm;
    sm.handle(Event::BootDone);
    sm.handle(Event::CheckPassed);
    sm.handle(Event::GuidedRequest);
    EXPECT_EQ(sm.mode(), ControlMode::Rc);
    sm.handle(Event::ArmRequest);
    sm.handle(Event::GuidedRequest); // still PreArm
    EXPECT_EQ(sm.mode(), ControlMode::Rc);
}

TEST(StateMachine, SwitchingModeNeutralizesOutputs)
{
    StateMachine sm(State::PreArm);
    sm.handle(Event::PreArmPassed);
    Transition t = sm.handle(Event::GuidedRequest);
    EXPECT_EQ(sm.state(), State::Armed);
    EXPECT_EQ(sm.mode(), ControlMode::Guided);
    EXPECT_TRUE(t.actions & Action::NeutralOutputs);
    t = sm.handle(Event::GuidedRequest); // no change
    EXPECT_EQ(t.actions, Action::None);
    sm.handle(Event::RcRequest);
    EXPECT_EQ(sm.mode(), ControlMode::Rc);
}

TEST(StateMachine, ModeResetsOnDisarmAndKill)
{
    StateMachine sm(State::PreArm);
    sm.handle(Event::PreArmPassed);
    sm.handle(Event::GuidedRequest);
    sm.handle(Event::KillAsserted);
    EXPECT_EQ(sm.mode(), ControlMode::Rc);
    sm.handle(Event::KillReleased);
    sm.handle(Event::SettleElapsed);
    sm.handle(Event::ArmRequest);
    sm.handle(Event::PreArmPassed);
    EXPECT_EQ(sm.state(), State::Armed);
    EXPECT_EQ(sm.mode(), ControlMode::Rc); // never re-arm into Guided
}

TEST(StateMachine, PreArmFailureOrDisarmReturnsToDisarmedWithoutRaisingKillLine)
{
    for (Event e : {Event::PreArmFailed, Event::DisarmRequest})
    {
        const Transition t = StateMachine::next(State::PreArm, e);
        EXPECT_EQ(t.next, State::Disarmed);
        EXPECT_FALSE(t.actions & Action::KillLineHigh);
    }
}

TEST(StateMachine, RecoverableFaultClearsToDisarmedButFaultDoesNot)
{
    for (State from : {State::Disarmed, State::PreArm, State::Armed})
    {
        const Transition t = StateMachine::next(from, Event::RecoverableFaultDetected);
        EXPECT_EQ(t.next, State::RecoverableFault);
        EXPECT_TRUE(t.actions & Action::KillLineLow);
    }
    EXPECT_EQ(StateMachine::next(State::RecoverableFault, Event::FaultCleared).next,
              State::Disarmed);
    EXPECT_EQ(StateMachine::next(State::RecoverableFault, Event::FaultDetected).next, State::Fault);
    EXPECT_EQ(StateMachine::next(State::Fault, Event::FaultCleared).next, State::Fault);
    // A kill outranks a recoverable fault.
    EXPECT_EQ(StateMachine::next(State::RecoverableFault, Event::KillAsserted).next, State::Killed);
}
