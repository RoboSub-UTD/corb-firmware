#pragma once

#include <cstddef>

#include "corb/command_gate.hpp"

namespace corb
{

// Output ports implemented by each platform (sim, chip). Core only ever talks
// to these abstractions.
class ThrusterOutput
{
public:
    virtual ~ThrusterOutput() = default;
    virtual void write(const ThrusterArray& values, std::size_t count) = 0;

protected:
    ThrusterOutput() = default;
    ThrusterOutput(const ThrusterOutput&) = default;
    ThrusterOutput& operator=(const ThrusterOutput&) = default;
};

class KillLine
{
public:
    virtual ~KillLine() = default;
    // true = run level (kill released), false = safe level (killed).
    virtual void set_run(bool run) = 0;

protected:
    KillLine() = default;
    KillLine(const KillLine&) = default;
    KillLine& operator=(const KillLine&) = default;
};

} // namespace corb
