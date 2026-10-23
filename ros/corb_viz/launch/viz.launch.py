"""Viz for a VCU on real hardware.

  ros2 launch corb_viz viz.launch.py agent:=true serial_dev:=/dev/ttyACM0
  ros2 launch corb_viz viz.launch.py                 # agent already running elsewhere

Arguments: agent, serial_dev, baud, rviz, commander (publish a test sine on the thrusters),
thruster_count.
"""

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, ExecuteProcess
from launch.conditions import IfCondition
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    share = get_package_share_directory("corb_viz")
    cfg = lambda name: LaunchConfiguration(name)  # noqa: E731

    return LaunchDescription(
        [
            DeclareLaunchArgument("agent", default_value="false"),
            DeclareLaunchArgument("serial_dev", default_value="/dev/ttyACM0"),
            DeclareLaunchArgument("baud", default_value="115200"),
            DeclareLaunchArgument("rviz", default_value="true"),
            DeclareLaunchArgument("commander", default_value="false"),
            DeclareLaunchArgument("thruster_count", default_value="4"),
            ExecuteProcess(
                condition=IfCondition(cfg("agent")),
                cmd=["ros2", "run", "micro_ros_agent", "micro_ros_agent", "serial",
                     "--dev", cfg("serial_dev"), "-b", cfg("baud")],
                output="screen",
            ),
            Node(
                package="corb_viz",
                executable="viz_node",
                parameters=[{"thruster_count": cfg("thruster_count")}],
                output="screen",
            ),
            Node(
                condition=IfCondition(cfg("commander")),
                package="corb_viz",
                executable="test_commander",
                parameters=[{"thruster_count": cfg("thruster_count")}],
            ),
            Node(
                package="tf2_ros",
                executable="static_transform_publisher",
                arguments=["--frame-id", "map", "--child-frame-id", "corb_base"],
            ),
            Node(
                condition=IfCondition(cfg("rviz")),
                package="rviz2",
                executable="rviz2",
                arguments=["-d", os.path.join(share, "config", "corb.rviz")],
            ),
        ]
    )
