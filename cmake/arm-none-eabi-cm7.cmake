# Cross toolchain for the STM32H723 (Cortex-M7, single-precision FPU hard-float).
# Set ARM_TOOLCHAIN_DIR (or have arm-none-eabi-gcc on PATH).
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

if(DEFINED ENV{ARM_TOOLCHAIN_DIR})
  set(_tc "$ENV{ARM_TOOLCHAIN_DIR}/bin/")
endif()
set(CMAKE_C_COMPILER   ${_tc}arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER ${_tc}arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER ${_tc}arm-none-eabi-gcc)
set(CMAKE_OBJCOPY      ${_tc}arm-none-eabi-objcopy)
set(CMAKE_SIZE         ${_tc}arm-none-eabi-size)

set(_cpu "-mcpu=cortex-m7 -mthumb -mfpu=fpv5-d16 -mfloat-abi=hard")
set(CMAKE_C_FLAGS_INIT   "${_cpu} -ffunction-sections -fdata-sections")
set(CMAKE_CXX_FLAGS_INIT "${_cpu} -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti -fno-threadsafe-statics")
set(CMAKE_ASM_FLAGS_INIT "${_cpu}")
set(CMAKE_EXE_LINKER_FLAGS_INIT "${_cpu} --specs=nano.specs --specs=nosys.specs -Wl,--gc-sections")
