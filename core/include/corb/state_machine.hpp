#pragma once

#include <cstddef>
#include <cstdint>

namespace corb
{

enum class State : std::uint8_t
{
    Boot,
    SelfCheck,
    Disarmed,
    PreArm, // arm requested; checks run before the kill line is raised
    Armed,
    Killed,
    Recovering,
    RecoverableFault, // outputs safe; clears back to Disarmed via FaultCleared
    Fault             // latched: only a reboot leaves it
};

constexpr std::size_t kStateCount = 9;
static_assert(static_cast<std::size_t>(State::Fault) + 1 == kStateCount,
              "kStateCount must match the State enumerators");

// Who is driving the thrusters while Armed. Orthogonal to State: it is only
// meaningful in State::Armed and always resets to Rc on entering or leaving it.
enum class ControlMode : std::uint8_t
{
    Rc,
    Guided
};

constexpr std::size_t kControlModeCount = 2;
static_assert(static_cast<std::size_t>(ControlMode::Guided) + 1 == kControlModeCount,
              "kControlModeCount must match the ControlMode enumerators");

enum class Event : std::uint8_t
{
    BootDone,
    CheckPassed,
    CheckFailed,
    ArmRequest,
    DisarmRequest,
    GuidedRequest,
    RcRequest,
    KillAsserted,
    KillReleased,
    SettleElapsed,
    PreArmPassed,
    PreArmFailed,
    RecoverableFaultDetected,
    FaultCleared,
    FaultDetected // keep last (see kEventCount)
};

constexpr std::size_t kEventCount = 15;
static_assert(static_cast<std::size_t>(Event::FaultDetected) + 1 == kEventCount,
              "kEventCount must match the Event enumerators");

// Bit flags describing what the platform must do on a transition.
namespace Action
{
constexpr std::uint8_t None = 0;
constexpr std::uint8_t NeutralOutputs = 1u << 0; // force all thrusters to 0
constexpr std::uint8_t KillLineLow = 1u << 1;    // drive kill line to safe (killed) level
constexpr std::uint8_t KillLineHigh = 1u << 2;   // drive kill line to run level
} // namespace Action

struct Transition
{
    State next;
    std::uint8_t actions;
};

// Owns the lifecycle state and the control mode. Events with no defined transition leave the
// state unchanged, except that KillAsserted and FaultDetected are always honoured
// (Fault is terminal).
class StateMachine
{
public:
    StateMachine() = default;
    explicit StateMachine(State initial) : state_(initial) {}

    State state() const { return state_; }
    ControlMode mode() const { return mode_; }

    // Applies the event, updates state and mode, and returns what the platform must do.
    // Switching mode also requests NeutralOutputs.
    Transition handle(Event ev);

    // Pure lookup, exposed so the full table can be tested exhaustively.
    static Transition next(State from, Event ev);

    // Pure lookup for the control mode after `ev` while in lifecycle state `in`.
    static ControlMode next_mode(State in, ControlMode current, Event ev);

private:
    State state_ = State::Boot;
    ControlMode mode_ = ControlMode::Rc;
};

const char* to_string(State s);
const char* to_string(Event e);

} // namespace corb
