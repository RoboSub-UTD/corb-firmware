# Shared by every platform. Add after the platform has defined the freertos_kernel target.
add_library(platform_common STATIC vcu_app.cpp)
target_include_directories(platform_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(platform_common PUBLIC vcu_core freertos_kernel)
vcu_set_warnings(platform_common)

# Linked by the platform after it provides micro-ROS (libmicroros.a) to `microros`.
if(TARGET microros)
  add_library(microros_bridge STATIC microros_bridge.cpp)
  target_link_libraries(microros_bridge PUBLIC platform_common microros)
  vcu_set_warnings(microros_bridge)
endif()
