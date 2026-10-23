# CORB-firmware (Cube Orange Replacement Board)

> [!NOTE]
> This is still very early in development

Firmware for vehicle control

## Building

Clone the repo:

```
git clone --recurse-submodules https://github.com/RoboSub-UTD/corb-firmware
```

## Build & test (laptop)

```
cmake --preset host-debug && cmake --build --preset host-debug && ctest --preset host-debug
cmake --preset host-asan  && cmake --build --preset host-asan  && ctest --preset host-asan
```

Run the sim: `build/host-debug/platform_sim/corb_firmware`.

## CLion

The CMake presets in `CMakePresets.json` are picked up by CLion automatically:

1. Open the repo root as a project (File > Open). Make sure the submodules are checked out first.
2. Go to Settings > Build, Execution, Deployment > CMake. CLion lists every preset (`host-debug`, `host-asan`, `stm32-debug`) as a profile, with a checkbox next to each.
3. Tick **Enable profile** for `host-debug` and `host-asan`. Leave the generated profile settings as they are: build type, generator (Ninja) and options come from the preset. Click OK.
4. CLion reloads CMake and creates the build directories `build/host-debug` and `build/host-asan`. Pick a profile and the `corb_firmware` or `vcu_tests` target in the run configuration dropdown at the top right, then build or run.
5. To run the tests, choose the `vcu_tests` target (or a GoogleTest run configuration) under the profile you want. ASan/UBSan findings show up in the Run window when the `host-asan` profile is selected.

If a profile doesn't appear, check that `cmake` and `ninja` are installed and that the CMake version is 3.22 or newer (the presets need it). If `host-debug` or `host-asan` shows as disabled, enable it in the same settings page and use Tools > CMake > Reload CMake Project.

`stm32-debug` needs `arm-none-eabi-gcc` and `ARM_TOOLCHAIN_DIR` set (see below), so leave it disabled unless you are building firmware from CLion.

## micro-ROS (simulator)

The ROS 2 stack talks to the VCU through a micro-ROS Agent over UDP.
Topics: `/vcu/thruster_cmd` (`corb_msgs/ThrusterCommand`, in) and `/vcu/state` (`corb_msgs/VcuState`, out).

One-time: `sudo rosdep init && rosdep update` (ROS 2 Lyrical at `/opt/ros/lyrical`).

```
tools/microros/build_libmicroros.sh                   # libmicroros.a + agent -> build/microros/
cmake --preset host-debug -DVCU_ENABLE_MICROROS=ON && cmake --build --preset host-debug
source build/microros/agent_ws/install/local_setup.bash
ros2 run micro_ros_agent micro_ros_agent udp4 --port 8888   # terminal 1
build/host-debug/platform_sim/corb_firmware                  # terminal 2
```

Override the agent address with `VCU_AGENT_IP` / `VCU_AGENT_PORT`.

## Visualizing a real MCU (`ros/corb_viz`)

Host-side ROS 2 package that shows the VCU live: an RViz view (state, link health, event log, one
bar per thruster), `/diagnostics`, and flat plot topics under `/corb_viz/` for PlotJuggler.
The VCU does not report applied outputs, so the bars show the commands on `/vcu/thruster_cmd`.

```
colcon build --packages-select corb_msgs corb_viz && source install/setup.bash
ros2 launch corb_viz viz.launch.py agent:=true serial_dev:=/dev/ttyACM0 baud:=115200
ros2 launch corb_viz viz.launch.py commander:=true   # also publish a small test sine on the thrusters
```

Without `agent:=true`, start the micro-ROS Agent yourself. Link loss, MCU reboots (uptime going
backwards) and state/`command_fresh` changes are logged in the view and in `/diagnostics`.
Logic tests (no ROS needed): `cd ros/corb_viz && PYTHONPATH=. python3 -m unittest test/test_link_monitor.py`.

## STM32H723 prototype board (FK723M1-ZGT6)

The production MCU is the RP2350; the STM32H723ZGT6 is only for prototyping the basic flows. Anything
chip-specific lives in `platform_stm32/`; `core/` and `platform_common/` are shared with the sim.

Toolchain: `arm-none-eabi-gcc` (Arm GNU 14.x). Either install it or unpack it and `export ARM_TOOLCHAIN_DIR=/path/to/arm-gnu-toolchain-...`.
Submodules (FreeRTOS, ST HAL, CMSIS) come with `git submodule update --init --depth 1`.

```
# 1. micro-ROS for Cortex-M7 (once; needs ROS 2 like the sim build)
TARGET=stm32 tools/microros/build_libmicroros.sh
# 2. firmware (add -DVCU_ENABLE_MICROROS=OFF to skip micro-ROS)
cmake --preset stm32-debug && cmake --build --preset stm32-debug
# 3. flash over SWD (ST-Link on the board's SWD header)
openocd -f interface/stlink.cfg -f target/stm32h7x.cfg \
  -c "program build/stm32-debug/platform_stm32/corb_firmware.elf verify reset exit"
```

| Function | Pins |
|---|---|
| Console (`printf`), 115200 8N1 | USART1 PA9 (TX) / PA10 (RX) |
| micro-ROS link (USB CDC-ACM, shows up as `/dev/ttyACM0`) | the board's USB-C port: PA11 (D-) / PA12 (D+) |
| Kill line (high = run, low = safe; low from reset) | PB0 |
| Thruster ESC PWM, 50 Hz, 1100-1900 us | TIM1 CH1-4: PE9, PE11, PE13, PE14 |
| Heartbeat LED (100 ms on / 900 ms off) | PG7 |

Plug the USB-C port into the laptop; the firmware enumerates as a CDC serial device (TinyUSB, so the
RP2350 build can reuse the transport). Run the Agent on it (baud is ignored over USB):
`ros2 run micro_ros_agent micro_ros_agent serial --dev /dev/ttyACM0`.
With the visualization node, its defaults work as they are:
`ros2 launch corb_viz viz.launch.py agent:=true commander:=true`.
VBUS sensing is off (the device always assumes it is attached), and USB runs from the HSI48 oscillator trimmed to the host's SOF packets, so it doesn't depend on the crystal.

Clocks assume the 15 MHz HSE stated in Zephyr's board docs (core 400 MHz). A solid-on LED at boot means the clock tree failed to start (wrong crystal value or no crystal): fix `HSE_VALUE` in `platform_stm32/stm32h7xx_hal_conf.h` and `PLLM` in `board.cpp`.
Firmware RAM is currently sized to 128 KB of AXI SRAM (`STM32H723ZG_FLASH.ld`); the chip has more.
