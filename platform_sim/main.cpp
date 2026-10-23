#include <cstdio>

#include "FreeRTOS.h"
#include "task.h"

#include "corb/ports.hpp"
#include "vcu_app.hpp"

#ifdef VCU_ENABLE_MICROROS
#include "udp_transport.hpp"
#endif

namespace
{

class SimThrusters final : public corb::ThrusterOutput
{
public:
    void write(const corb::ThrusterArray& values, std::size_t) override { first_ = values[0]; }
    const float* first() const { return &first_; }

private:
    float first_ = 0.0f;
};

class SimKillLine final : public corb::KillLine
{
public:
    void set_run(bool run) override { std::printf("kill line -> %s\n", run ? "RUN" : "SAFE"); }
};

} // namespace

int main()
{
    static SimThrusters thrusters;
    static SimKillLine kill_line;
#ifdef VCU_ENABLE_MICROROS
    static platform::VcuApp app(thrusters, kill_line, {false});
    platform::start_microros(app, sim::udp_transport());
#else
    static platform::VcuApp app(thrusters, kill_line, {true});
#endif
    app.set_log_probe(thrusters.first());
    app.start_control_task();
    vTaskStartScheduler();
    return 0;
}
