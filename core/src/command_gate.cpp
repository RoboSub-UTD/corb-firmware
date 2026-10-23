#include "corb/command_gate.hpp"

#include <cmath>

namespace corb
{

bool CommandGate::submit(std::uint32_t seq, std::uint32_t now_ms, const float* values,
                         std::size_t n)
{
    if (values == nullptr || n != count_)
    {
        return false;
    }
    // Validate everything before touching state so acceptance is atomic.
    for (std::size_t i = 0; i < n; ++i)
    {
        if (!std::isfinite(values[i]) || std::fabs(values[i]) > 1.0f)
        {
            return false;
        }
    }
    if (have_command_ && static_cast<std::int32_t>(seq - last_seq_) <= 0)
    {
        return false;
    }

    ThrusterArray next{};
    for (std::size_t i = 0; i < n; ++i)
    {
        next[i] = values[i];
    }
    command_ = next;
    last_seq_ = seq;
    last_rx_ms_ = now_ms;
    have_command_ = true;
    return true;
}

bool CommandGate::fresh(std::uint32_t now_ms) const
{
    return have_command_ && static_cast<std::uint32_t>(now_ms - last_rx_ms_) < timeout_ms_;
}

ThrusterArray CommandGate::sample(std::uint32_t now_ms) const
{
    if (fresh(now_ms))
    {
        return command_;
    }
    return ThrusterArray{};
}

void CommandGate::reset()
{
    have_command_ = false;
    command_ = ThrusterArray{};
}

} // namespace corb
