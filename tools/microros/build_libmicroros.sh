#!/usr/bin/env bash
# Builds libmicroros.a (+ headers) and the micro-ROS Agent.
#   (default)       host build for the simulator -> build/microros/
#   TARGET=stm32    Cortex-M7 build for the STM32H723 (serial transport, no agent)
#                   -> build/microros-stm32/ ; needs arm-none-eabi-gcc (ARM_TOOLCHAIN_DIR or PATH)
# Outputs: firmware/build/{libmicroros.a,include}, agent_ws/ (host only).
# One-time: sudo rosdep init && rosdep update
# Requires a sourced-able ROS 2 install (default /opt/ros/lyrical), colcon, git.
set -euo pipefail

ROS_DISTRO_NAME="${ROS_DISTRO_NAME:-lyrical}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
TARGET="${TARGET:-host}"
HERE="$ROOT/tools/microros"
case "$TARGET" in
  host)  OUT="$ROOT/build/microros";       TOOLCHAIN="$HERE/host_toolchain.cmake" ;;
  stm32) OUT="$ROOT/build/microros-stm32"; TOOLCHAIN="$HERE/stm32_toolchain.cmake" ;;
  *) echo "unknown TARGET=$TARGET (host|stm32)" >&2; exit 1 ;;
esac

set +u
# shellcheck disable=SC1090
source "/opt/ros/$ROS_DISTRO_NAME/setup.bash"
set -u

mkdir -p "$OUT/setup_ws/src"
cd "$OUT/setup_ws"
[ -d src/micro_ros_setup ] || git clone --depth 1 -b "$ROS_DISTRO_NAME" \
  https://github.com/micro-ROS/micro_ros_setup src/micro_ros_setup
colcon build --packages-select micro_ros_setup
set +u
# shellcheck disable=SC1091
source install/local_setup.bash
set -u

# ---- client library (libmicroros.a) ----
cd "$OUT"
if [ ! -f firmware/.created ]; then
  rm -rf firmware
  ros2 run micro_ros_setup create_firmware_ws.sh generate_lib
  touch firmware/.created
fi
rm -rf firmware/mcu_ws/extra_packages/corb_msgs
mkdir -p firmware/mcu_ws/extra_packages
cp -r "$ROOT/msgs/corb_msgs" firmware/mcu_ws/extra_packages/
ros2 run micro_ros_setup build_firmware.sh "$TOOLCHAIN" "$HERE/colcon.meta"

if [ "$TARGET" = stm32 ]; then
  echo "libmicroros: $OUT/firmware/build/libmicroros.a"
  echo "build fw:    cmake --preset stm32-debug && cmake --build --preset stm32-debug"
  exit 0
fi

# ---- agent (talks UDP to the sim, DDS to the ROS 2 graph) ----
mkdir -p agent_ws
cd agent_ws
if [ ! -f .created ]; then
  rm -rf src
  ros2 run micro_ros_setup create_agent_ws.sh
  touch .created
fi
ros2 run micro_ros_setup build_agent.sh

echo "libmicroros: $OUT/firmware/build/libmicroros.a"
echo "run agent:   source $OUT/agent_ws/install/local_setup.bash && ros2 run micro_ros_agent micro_ros_agent udp4 --port 8888"
