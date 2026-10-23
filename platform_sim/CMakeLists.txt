set(FREERTOS_PORT GCC_POSIX CACHE STRING "" FORCE)
set(FREERTOS_HEAP 4 CACHE STRING "" FORCE)

# Kernel's CMake expects this target to provide FreeRTOSConfig.h.
add_library(freertos_config INTERFACE)
target_include_directories(freertos_config SYSTEM INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(${PROJECT_SOURCE_DIR}/third_party/FreeRTOS-Kernel
                 ${CMAKE_BINARY_DIR}/freertos_kernel)

add_executable(corb_firmware main.cpp)
target_link_libraries(corb_firmware PRIVATE vcu_core freertos_kernel)
vcu_set_warnings(corb_firmware)

# ---- optional micro-ROS link (run tools/microros/build_libmicroros.sh first) ----
option(VCU_ENABLE_MICROROS "Link micro-ROS into the simulator" OFF)
if(VCU_ENABLE_MICROROS)
  set(VCU_MICROROS_DIR "${PROJECT_SOURCE_DIR}/build/microros/firmware/build"
      CACHE PATH "Directory holding libmicroros.a and include/")
  if(NOT EXISTS "${VCU_MICROROS_DIR}/libmicroros.a")
    message(FATAL_ERROR "libmicroros.a not found in ${VCU_MICROROS_DIR}; "
                        "run tools/microros/build_libmicroros.sh")
  endif()
  add_library(microros STATIC IMPORTED)
  set_target_properties(microros PROPERTIES
    IMPORTED_LOCATION "${VCU_MICROROS_DIR}/libmicroros.a")
  target_include_directories(microros SYSTEM INTERFACE "${VCU_MICROROS_DIR}/include")

  target_sources(corb_firmware PRIVATE microros_bridge.cpp)
  target_link_libraries(corb_firmware PRIVATE microros)
  target_compile_definitions(corb_firmware PRIVATE VCU_ENABLE_MICROROS=1)
endif()
