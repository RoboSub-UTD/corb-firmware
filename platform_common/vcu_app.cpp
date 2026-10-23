#include "vcu_app.hpp"

#include <cstdio>

#include "task.h"

namespace platform
{

void VcuApp::start_control_task()
{
    xTaskCreate(&VcuApp::control_entry, "control", configMINIMAL_STACK_SIZE, this, 2, nullptr);
}

bool VcuApp::on_command(std::uint32_t seq, const float* values, std::size_t n)
{
    const Mutex::Guard guard(lock_);
    return vcu_.submit_command(seq, now_ms(), values, n);
}

void VcuApp::snapshot(std::uint8_t& state, bool& command_fresh)
{
    const Mutex::Guard guard(lock_);
    state = static_cast<std::uint8_t>(vcu_.state());
    command_fresh = vcu_.command_fresh(now_ms());
}

void VcuApp::dispatch(corb::Event ev)
{
    const Mutex::Guard guard(lock_);
    const corb::State from = vcu_.state();
    vcu_.handle(ev);
    std::printf("%s --%s--> %s\n", corb::to_string(from), corb::to_string(ev),
                corb::to_string(vcu_.state()));
}

// Ticks the Vcu until the pre-arm checks pass (or give up after 2 s).
void VcuApp::wait_until_armed()
{
    for (int i = 0; i < 200; ++i)
    {
        {
            const Mutex::Guard guard(lock_);
            const corb::State before = vcu_.state();
            vcu_.tick(now_ms());
            if (vcu_.state() != before)
            {
                std::printf("%s -> %s\n", corb::to_string(before), corb::to_string(vcu_.state()));
            }
            if (vcu_.state() == corb::State::Armed)
            {
                return;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void VcuApp::control_loop()
{
    dispatch(corb::Event::BootDone);
    dispatch(corb::Event::CheckPassed);
    {
        const Mutex::Guard guard(lock_);
        vcu_.set_health({true, true}); // no RC receiver yet: pretend the RC link and battery are fine
    }
    dispatch(corb::Event::ArmRequest);
    wait_until_armed();
    dispatch(corb::Event::GuidedRequest);

    TickType_t last_wake = xTaskGetTickCount();
    for (;;)
    {
        const std::uint32_t now = now_ms();
        if (opts_.feed_test_command && now < 1000)
        {
            const float cmd[4] = {0.5f, 0.5f, -0.5f, -0.5f};
            const Mutex::Guard guard(lock_);
            (void)vcu_.submit_command(++seq_, now, cmd, 4);
        }
        {
            const Mutex::Guard guard(lock_);
            vcu_.tick(now);
            if (now % 250 == 0)
            {
                std::printf("t=%u state=%s fresh=%d t0=%.2f\n", static_cast<unsigned>(now),
                            corb::to_string(vcu_.state()), vcu_.command_fresh(now) ? 1 : 0,
                            probe_ ? static_cast<double>(*probe_) : 0.0);
            }
        }
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(10));
    }
}

} // namespace platform
