#include <cstdint>
#include <cstdio>

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#include "corb/ports.hpp"
#include "corb/vcu.hpp"

#ifdef VCU_ENABLE_MICROROS
#include "microros_bridge.hpp"
#endif

namespace
{

// RAII wrapper over a FreeRTOS mutex.
class Mutex
{
public:
    Mutex() : handle_(xSemaphoreCreateMutex()) {}
    ~Mutex() { vSemaphoreDelete(handle_); }
    Mutex(const Mutex&) = delete;
    Mutex& operator=(const Mutex&) = delete;

    class Guard
    {
    public:
        explicit Guard(Mutex& m) : m_(m) { xSemaphoreTake(m_.handle_, portMAX_DELAY); }
        ~Guard() { xSemaphoreGive(m_.handle_); }
        Guard(const Guard&) = delete;
        Guard& operator=(const Guard&) = delete;

    private:
        Mutex& m_;
    };

private:
    SemaphoreHandle_t handle_;
};

class SimThrusters final : public corb::ThrusterOutput
{
public:
    void write(const corb::ThrusterArray& values, std::size_t) override { last_ = values; }
    float first() const { return last_[0]; }

private:
    corb::ThrusterArray last_{};
};

class SimKillLine final : public corb::KillLine
{
public:
    void set_run(bool run) override { std::printf("kill line -> %s\n", run ? "RUN" : "SAFE"); }
};

// Owns the simulated platform: ports, the Vcu, and the lock that serializes
// the control task and the micro-ROS task.
#ifdef VCU_ENABLE_MICROROS
class SimApp final : public sim::BridgeClient
#else
class SimApp final
#endif
{
public:
    SimApp() : vcu_(thrusters_, kill_line_, corb::Vcu::Config{}) {}

    void run()
    {
#ifdef VCU_ENABLE_MICROROS
        sim::start_microros(*this);
#endif
        xTaskCreate(&SimApp::control_entry, "control", configMINIMAL_STACK_SIZE, this, 2, nullptr);
        vTaskStartScheduler();
    }

#ifdef VCU_ENABLE_MICROROS
    bool on_command(std::uint32_t seq, const float* values, std::size_t n) override
    {
        const Mutex::Guard guard(lock_);
        return vcu_.submit_command(seq, now_ms(), values, n);
    }

    void snapshot(std::uint8_t& state, bool& command_fresh) override
    {
        const Mutex::Guard guard(lock_);
        state = static_cast<std::uint8_t>(vcu_.state());
        command_fresh = vcu_.command_fresh(now_ms());
    }
#endif

private:
    static std::uint32_t now_ms() { return static_cast<std::uint32_t>(xTaskGetTickCount()); }

    static void control_entry(void* self) { static_cast<SimApp*>(self)->control_loop(); }

    void dispatch(corb::Event ev)
    {
        const Mutex::Guard guard(lock_);
        const corb::State from = vcu_.state();
        vcu_.handle(ev);
        std::printf("%s --%s--> %s\n", corb::to_string(from), corb::to_string(ev),
                    corb::to_string(vcu_.state()));
    }

    // Ticks the Vcu until the pre-arm checks pass (or give up after 2 s).
    void wait_until_armed()
    {
        for (int i = 0; i < 200; ++i)
        {
            {
                const Mutex::Guard guard(lock_);
                const corb::State before = vcu_.state();
                vcu_.tick(now_ms());
                if (vcu_.state() != before)
                {
                    std::printf("%s -> %s\n", corb::to_string(before),
                                corb::to_string(vcu_.state()));
                }
                if (vcu_.state() == corb::State::Armed)
                {
                    return;
                }
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }

    void control_loop()
    {
        dispatch(corb::Event::BootDone);
        dispatch(corb::Event::CheckPassed);
        {
            const Mutex::Guard guard(lock_);
            vcu_.set_health({true, true}); // simulated RC link and battery are always fine
        }
        dispatch(corb::Event::ArmRequest);
        wait_until_armed();
        dispatch(corb::Event::GuidedRequest);

        TickType_t last_wake = xTaskGetTickCount();
        for (;;)
        {
            const std::uint32_t now = now_ms();
#ifndef VCU_ENABLE_MICROROS
            // Stand-in for the ROS 2 link: feed a command for the first second only.
            if (now < 1000)
            {
                const float cmd[4] = {0.5f, 0.5f, -0.5f, -0.5f};
                const Mutex::Guard guard(lock_);
                (void)vcu_.submit_command(++seq_, now, cmd, 4);
            }
#endif
            {
                const Mutex::Guard guard(lock_);
                vcu_.tick(now);
                if (now % 250 == 0)
                {
                    std::printf("t=%u fresh=%d t0=%.2f\n", static_cast<unsigned>(now),
                                vcu_.command_fresh(now) ? 1 : 0,
                                static_cast<double>(thrusters_.first()));
                }
            }
            vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(10));
        }
    }

    Mutex lock_;
    SimThrusters thrusters_;
    SimKillLine kill_line_;
    corb::Vcu vcu_;
#ifndef VCU_ENABLE_MICROROS
    std::uint32_t seq_ = 0;
#endif
};

} // namespace

int main()
{
    static SimApp app;
    app.run();
    return 0;
}
