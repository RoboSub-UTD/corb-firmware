cmake_minimum_required(VERSION 3.22)
project(corb_msgs)

find_package(ament_cmake REQUIRED)
find_package(rosidl_default_generators REQUIRED)

rosidl_generate_interfaces(${PROJECT_NAME}
  "msg/ThrusterCommand.msg"
  "msg/VcuState.msg")

ament_package()
