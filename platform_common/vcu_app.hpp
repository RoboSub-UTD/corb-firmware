#pragma once

#include <cstdint>

#include "FreeRTOS.h"
#include "semphr.h"

#include "corb/ports.hpp"
#include "corb/vcu.hpp"
#include "microros_bridge.hpp"

namespace platform
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

// The application shared by every platform: owns the Vcu and the lock that
// serializes the control task and the micro-ROS task. Platforms supply the ports.
class VcuApp final : public BridgeClient
{
public:
    struct Options
    {
        // Stand-in for the ROS 2 link: feed a fixed command for the first second.
        bool feed_test_command = false;
    };

    VcuApp(corb::ThrusterOutput& thrusters, corb::KillLine& kill_line, const Options& opts)
        : opts_(opts), vcu_(thrusters, kill_line, corb::Vcu::Config{})
    {
    }

    // Creates the control task. The caller starts the scheduler.
    void start_control_task();

    bool on_command(std::uint32_t seq, const float* values, std::size_t n) override;
    void snapshot(std::uint8_t& state, bool& command_fresh) override;

    // Last value written to thruster 0, for periodic logging.
    void set_log_probe(const float* probe) { probe_ = probe; }

private:
    static std::uint32_t now_ms() { return static_cast<std::uint32_t>(xTaskGetTickCount()); }
    static void control_entry(void* self) { static_cast<VcuApp*>(self)->control_loop(); }

    void dispatch(corb::Event ev);
    void wait_until_armed();
    void control_loop();

    Options opts_;
    Mutex lock_;
    corb::Vcu vcu_;
    const float* probe_ = nullptr;
    std::uint32_t seq_ = 0;
};

} // namespace platform
