# CORB-firmware (Cube Orange Replacement Board)

> [!This is still very early in development]

Firmware for vehicle control

## Building

Clone the repo:

```
git clone --recurse-submodules <url>
```

## Build & test (laptop)

```
cmake --preset host-debug && cmake --build --preset host-debug && ctest --preset host-debug
cmake --preset host-asan  && cmake --build --preset host-asan  && ctest --preset host-asan
```

Run the sim: `build/host-debug/platform_sim/corb_firmware`.

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
