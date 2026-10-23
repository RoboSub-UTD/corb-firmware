#include "corb/state_machine.hpp"

namespace corb
{

namespace
{
constexpr std::uint8_t kSafe = Action::NeutralOutputs | Action::KillLineLow;
}

Transition StateMachine::next(State from, Event ev)
{
    // Fault is terminal: only a reboot leaves it.
    if (from == State::Fault)
    {
        return {State::Fault, kSafe};
    }
    // Safety events win in every other state.
    if (ev == Event::FaultDetected)
    {
        return {State::Fault, kSafe};
    }
    if (ev == Event::KillAsserted)
    {
        return {State::Killed, kSafe};
    }

    switch (from)
    {
    case State::Boot:
        if (ev == Event::BootDone)
            return {State::SelfCheck, Action::NeutralOutputs};
        break;
    case State::SelfCheck:
        if (ev == Event::CheckPassed)
            return {State::Disarmed, Action::NeutralOutputs};
        if (ev == Event::CheckFailed)
            return {State::Fault, kSafe};
        break;
    case State::Disarmed:
        if (ev == Event::ArmRequest)
            return {State::PreArm, Action::NeutralOutputs};
        if (ev == Event::RecoverableFaultDetected)
            return {State::RecoverableFault, kSafe};
        break;
    case State::PreArm:
        if (ev == Event::PreArmPassed)
            return {State::Armed, Action::KillLineHigh};
        if (ev == Event::PreArmFailed || ev == Event::DisarmRequest)
            return {State::Disarmed, Action::NeutralOutputs};
        if (ev == Event::RecoverableFaultDetected)
            return {State::RecoverableFault, kSafe};
        break;
    case State::Armed:
        if (ev == Event::DisarmRequest)
            return {State::Disarmed, kSafe};
        if (ev == Event::RecoverableFaultDetected)
            return {State::RecoverableFault, kSafe};
        break;
    case State::RecoverableFault:
        if (ev == Event::FaultCleared)
            return {State::Disarmed, kSafe};
        break;
    case State::Killed:
        if (ev == Event::KillReleased)
            return {State::Recovering, Action::NeutralOutputs};
        break;
    case State::Recovering:
        if (ev == Event::SettleElapsed)
            return {State::Disarmed, Action::NeutralOutputs};
        break;
    case State::Fault:
        break;
    }

    // Undefined: stay put. Anything that is not an armed state stays neutral.
    const bool armed = (from == State::Armed);
    return {from, armed ? Action::None : Action::NeutralOutputs};
}

Transition StateMachine::handle(Event ev)
{
    Transition t = next(state_, ev);
    const ControlMode previous = mode_;
    // The mode resets whenever the lifecycle state changes.
    mode_ = (t.next != state_) ? ControlMode::Rc : next_mode(state_, mode_, ev);
    if (mode_ != previous)
    {
        t.actions = static_cast<std::uint8_t>(t.actions | Action::NeutralOutputs);
    }
    state_ = t.next;
    return t;
}

ControlMode StateMachine::next_mode(State in, ControlMode current, Event ev)
{
    if (in != State::Armed)
    {
        return ControlMode::Rc;
    }
    if (ev == Event::GuidedRequest)
    {
        return ControlMode::Guided;
    }
    if (ev == Event::RcRequest)
    {
        return ControlMode::Rc;
    }
    return current;
}

const char* to_string(ControlMode m)
{
    switch (m)
    {
    case ControlMode::Rc:
        return "Rc";
    case ControlMode::Guided:
        return "Guided";
    }
    return "?";
}

const char* to_string(State s)
{
    switch (s)
    {
    case State::Boot:
        return "Boot";
    case State::SelfCheck:
        return "SelfCheck";
    case State::Disarmed:
        return "Disarmed";
    case State::PreArm:
        return "PreArm";
    case State::RecoverableFault:
        return "RecoverableFault";
    case State::Armed:
        return "Armed";
    case State::Killed:
        return "Killed";
    case State::Recovering:
        return "Recovering";
    case State::Fault:
        return "Fault";
    }
    return "?";
}

const char* to_string(Event e)
{
    switch (e)
    {
    case Event::BootDone:
        return "BootDone";
    case Event::CheckPassed:
        return "CheckPassed";
    case Event::CheckFailed:
        return "CheckFailed";
    case Event::ArmRequest:
        return "ArmRequest";
    case Event::DisarmRequest:
        return "DisarmRequest";
    case Event::GuidedRequest:
        return "GuidedRequest";
    case Event::RcRequest:
        return "RcRequest";
    case Event::KillAsserted:
        return "KillAsserted";
    case Event::KillReleased:
        return "KillReleased";
    case Event::SettleElapsed:
        return "SettleElapsed";
    case Event::PreArmPassed:
        return "PreArmPassed";
    case Event::PreArmFailed:
        return "PreArmFailed";
    case Event::RecoverableFaultDetected:
        return "RecoverableFaultDetected";
    case Event::FaultCleared:
        return "FaultCleared";
    case Event::FaultDetected:
        return "FaultDetected";
    }
    return "?";
}

} // namespace corb
