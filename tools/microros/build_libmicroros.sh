#!/usr/bin/env bash
# Builds libmicroros.a (+ headers) for the host and the micro-ROS Agent.
# Outputs under build/microros/: firmware/build/{libmicroros.a,include}, agent_ws/.
# One-time: sudo rosdep init && rosdep update
# Requires a sourced-able ROS 2 install (default /opt/ros/lyrical), colcon, git.
set -euo pipefail

ROS_DISTRO_NAME="${ROS_DISTRO_NAME:-lyrical}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT="$ROOT/build/microros"
HERE="$ROOT/tools/microros"

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
ros2 run micro_ros_setup build_firmware.sh "$HERE/host_toolchain.cmake" "$HERE/colcon.meta"

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
