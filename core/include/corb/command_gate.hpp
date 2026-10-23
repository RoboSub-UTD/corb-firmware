#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace corb
{

constexpr std::size_t kMaxThrusters = 8;

// Normalized thruster commands in [-1, 1]; 0 is neutral.
using ThrusterArray = std::array<float, kMaxThrusters>;

// Validates and time-gates per-thruster commands. A stale command is never
// held: sample() returns all zeros once the timeout has elapsed.
class CommandGate
{
public:
    struct Config
    {
        std::uint32_t timeout_ms = 250;
        std::size_t thruster_count = 4;
    };

    CommandGate() : CommandGate(Config{}) {}
    explicit CommandGate(const Config& cfg)
        : timeout_ms_(cfg.timeout_ms),
          count_(cfg.thruster_count > kMaxThrusters ? kMaxThrusters : cfg.thruster_count)
    {
    }

    // Returns true if accepted. A rejected message leaves all state untouched
    // (in particular it does not refresh the timer).
    bool submit(std::uint32_t seq, std::uint32_t now_ms, const float* values, std::size_t n);

    bool fresh(std::uint32_t now_ms) const;

    ThrusterArray sample(std::uint32_t now_ms) const;

    void reset();

private:
    std::uint32_t timeout_ms_;
    std::size_t count_;
    bool have_command_ = false;
    std::uint32_t last_seq_ = 0;
    std::uint32_t last_rx_ms_ = 0;
    ThrusterArray command_{};
};

} // namespace corb
