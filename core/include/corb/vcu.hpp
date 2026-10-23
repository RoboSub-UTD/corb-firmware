#pragma once

#include <cstddef>
#include <cstdint>

#include "corb/command_gate.hpp"
#include "corb/ports.hpp"
#include "corb/state_machine.hpp"

namespace corb
{

// Where a kill came from. Bit flags: several sources can hold the kill at once,
// and each can only release its own. The hardware kill button is deliberately not a
// source: it cuts the kill line in hardware and the board has no input to sense it.
enum class KillSource : std::uint8_t
{
    Wireless = 1u << 0,
    Software = 1u << 1,
    Heartbeat = 1u << 2
};

// Platform-reported health. Orthogonal to State: Vcu reacts to it in tick().
// The ROS link is not here: it is derived from the command gate's freshness.
struct Health
{
    bool rc_link_ok = false;
    bool battery_ok = true;
};

// Composes the command gate and state machine and drives the output ports.
// Not thread-safe: the platform serializes access. Time is always passed in.
class Vcu
{
public:
    struct Config
    {
        CommandGate::Config gate{};
        std::uint32_t prearm_timeout_ms = 1000; // PreArm fails if checks don't pass in time
        std::uint32_t rc_loss_disarm_ms = 2000; // Armed+Rc: disarm after RC link lost this long
    };

    Vcu(ThrusterOutput& thrusters, KillLine& kill_line, const Config& cfg)
        : thrusters_(thrusters), kill_line_(kill_line), cfg_(cfg), gate_(cfg.gate)
    {
    }

    // Applies a lifecycle/mode event and executes the resulting actions on the ports.
    // Prefer assert_kill / release_kill for kill events so sources are tracked.
    void handle(Event ev);

    // Kill handling. The VCU stays Killed until every asserting source has released.
    void assert_kill(KillSource source);
    void release_kill(KillSource source);
    std::uint8_t kill_sources() const { return kill_sources_; }

    void set_health(const Health& health) { health_ = health; }

    bool submit_command(std::uint32_t seq, std::uint32_t now_ms, const float* values, std::size_t n)
    {
        return gate_.submit(seq, now_ms, values, n);
    }

    // Periodic update: evaluates pre-arm checks and link/battery policy, then drives the
    // outputs (guided command only while Armed in Guided mode and fresh, neutral otherwise).
    void tick(std::uint32_t now_ms);

    State state() const { return machine_.state(); }
    ControlMode mode() const { return machine_.mode(); }
    bool command_fresh(std::uint32_t now_ms) const { return gate_.fresh(now_ms); }

private:
    void apply(std::uint8_t actions);
    void run_policy(std::uint32_t now_ms);

    ThrusterOutput& thrusters_;
    KillLine& kill_line_;
    Config cfg_;
    CommandGate gate_;
    StateMachine machine_;
    Health health_{};
    std::uint8_t kill_sources_ = 0;

    bool prearm_started_ = false;
    std::uint32_t prearm_start_ms_ = 0;
    bool rc_lost_ = false;
    std::uint32_t rc_lost_since_ms_ = 0;
    bool guided_link_seen_ = false; // a fresh command arrived during this Guided session
};

} // namespace corb
