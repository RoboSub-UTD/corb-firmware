# Cortex-M7 (STM32H723) toolchain for building libmicroros.a. Keep the CPU flags in sync with
# cmake/arm-none-eabi-cm7.cmake.
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

if(DEFINED ENV{ARM_TOOLCHAIN_DIR})
  set(_tc "$ENV{ARM_TOOLCHAIN_DIR}/bin/")
endif()
set(CMAKE_C_COMPILER   ${_tc}arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER ${_tc}arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER ${_tc}arm-none-eabi-gcc)
set(CMAKE_AR           ${_tc}arm-none-eabi-ar)

set(_cpu "-mcpu=cortex-m7 -mthumb -mfpu=fpv5-d16 -mfloat-abi=hard")
set(CMAKE_C_FLAGS_INIT   "-std=c11 ${_cpu} -ffunction-sections -fdata-sections --specs=nosys.specs -DCLOCK_MONOTONIC=1" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_INIT "-std=c++17 ${_cpu} -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti --specs=nosys.specs -DCLOCK_MONOTONIC=1" CACHE STRING "" FORCE)
