#include "corb/vcu.hpp"

namespace corb
{

void Vcu::handle(Event ev)
{
    apply(machine_.handle(ev).actions);
}

void Vcu::assert_kill(KillSource source)
{
    kill_sources_ = static_cast<std::uint8_t>(kill_sources_ | static_cast<std::uint8_t>(source));
    handle(Event::KillAsserted);
}

void Vcu::release_kill(KillSource source)
{
    kill_sources_ = static_cast<std::uint8_t>(kill_sources_ & ~static_cast<std::uint8_t>(source));
    if (kill_sources_ == 0)
    {
        handle(Event::KillReleased);
    }
}

void Vcu::apply(std::uint8_t actions)
{
    // Safe actions first: never raise the kill line before outputs are neutral.
    if (actions & Action::NeutralOutputs)
    {
        thrusters_.write(ThrusterArray{}, kMaxThrusters);
    }
    if (actions & Action::KillLineLow)
    {
        kill_line_.set_run(false);
    }
    if (actions & Action::KillLineHigh)
    {
        kill_line_.set_run(true);
    }
}

void Vcu::run_policy(std::uint32_t now_ms)
{
    const bool rc_needed = machine_.state() == State::Armed && machine_.mode() == ControlMode::Rc;

    switch (machine_.state())
    {
    case State::PreArm:
        if (!prearm_started_)
        {
            prearm_started_ = true;
            prearm_start_ms_ = now_ms;
        }
        if (health_.rc_link_ok && health_.battery_ok)
        {
            handle(Event::PreArmPassed);
        }
        else if (static_cast<std::uint32_t>(now_ms - prearm_start_ms_) >= cfg_.prearm_timeout_ms)
        {
            handle(Event::PreArmFailed);
        }
        break;

    case State::Armed:
        if (!health_.battery_ok)
        {
            handle(Event::RecoverableFaultDetected);
        }
        else if (machine_.mode() == ControlMode::Guided)
        {
            // Lost ROS link: fall back to RC (switching mode neutralizes outputs).
            if (gate_.fresh(now_ms))
            {
                guided_link_seen_ = true;
            }
            else if (guided_link_seen_)
            {
                handle(Event::RcRequest);
            }
        }
        break;

    case State::RecoverableFault:
        if (health_.battery_ok)
        {
            handle(Event::FaultCleared);
        }
        break;

    default:
        break;
    }

    if (machine_.state() != State::PreArm)
    {
        prearm_started_ = false;
    }
    if (machine_.mode() != ControlMode::Guided)
    {
        guided_link_seen_ = false;
    }

    // RC link loss only matters while RC is driving; disarm if it persists.
    if (rc_needed && !health_.rc_link_ok)
    {
        if (!rc_lost_)
        {
            rc_lost_ = true;
            rc_lost_since_ms_ = now_ms;
        }
        if (static_cast<std::uint32_t>(now_ms - rc_lost_since_ms_) >= cfg_.rc_loss_disarm_ms)
        {
            handle(Event::DisarmRequest);
        }
    }
    else
    {
        rc_lost_ = false;
    }
}

void Vcu::tick(std::uint32_t now_ms)
{
    run_policy(now_ms);

    if (machine_.state() == State::Armed && machine_.mode() == ControlMode::Guided)
    {
        thrusters_.write(gate_.sample(now_ms), kMaxThrusters);
    }
    else
    {
        thrusters_.write(ThrusterArray{}, kMaxThrusters);
    }
}

} // namespace corb
