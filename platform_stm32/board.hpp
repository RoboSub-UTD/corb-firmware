#pragma once

#include <cstddef>
#include <cstdint>

#include "corb/ports.hpp"
#include "microros_bridge.hpp"

// FK723M1-ZGT6 (STM32H723ZGT6) bring-up board support.
//
//   PG7        user LED (active high)
//   PA9/PA10   USART1 TX/RX, 115200 8N1: console (printf), on the board's debug header
//   PA11/PA12  USB D-/D+ (the USB-C port): CDC-ACM serial for the micro-ROS Agent
//   PB0        kill line (high = RUN, low = SAFE); low from reset onward
//   PE9/11/13/14  TIM1 CH1..4: thruster ESC PWM, 50 Hz, 1100..1900 us (1500 us neutral)
namespace board
{

// Clocks (HSE 15 MHz -> 400 MHz core), GPIO and peripherals. Call first in main().
void init();

void led_set(bool on);

// Starts the USB device task (call before the scheduler runs).
void start_usb();

// micro-ROS link over USB CDC.
const platform::Transport& microros_transport();

class Thrusters final : public corb::ThrusterOutput
{
public:
    void write(const corb::ThrusterArray& values, std::size_t count) override;
    const float* first() const { return &first_; }

private:
    float first_ = 0.0f;
};

class KillLine final : public corb::KillLine
{
public:
    void set_run(bool run) override;
};

} // namespace board
