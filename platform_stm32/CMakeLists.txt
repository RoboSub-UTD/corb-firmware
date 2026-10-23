set(FREERTOS_PORT GCC_ARM_CM7 CACHE STRING "" FORCE)
set(FREERTOS_HEAP 4 CACHE STRING "" FORCE)

set(HAL_DIR ${PROJECT_SOURCE_DIR}/third_party/stm32h7xx_hal_driver)
set(CMSIS_DEV_DIR ${PROJECT_SOURCE_DIR}/third_party/cmsis_device_h7)
set(CMSIS_CORE_DIR ${PROJECT_SOURCE_DIR}/third_party/cmsis_core/CMSIS/Core)

enable_language(ASM)

# Kernel's CMake expects this target to provide FreeRTOSConfig.h.
add_library(freertos_config INTERFACE)
target_include_directories(freertos_config SYSTEM INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(freertos_config INTERFACE STM32H723xx)

add_subdirectory(${PROJECT_SOURCE_DIR}/third_party/FreeRTOS-Kernel
                 ${CMAKE_BINARY_DIR}/freertos_kernel)

# ---- vendor code: HAL (RCC/PWR/GPIO/Cortex only) + CMSIS device + startup ----
add_library(stm32_hal STATIC
  ${HAL_DIR}/Src/stm32h7xx_hal.c
  ${HAL_DIR}/Src/stm32h7xx_hal_cortex.c
  ${HAL_DIR}/Src/stm32h7xx_hal_flash.c
  ${HAL_DIR}/Src/stm32h7xx_hal_flash_ex.c
  ${HAL_DIR}/Src/stm32h7xx_hal_gpio.c
  ${HAL_DIR}/Src/stm32h7xx_hal_pwr.c
  ${HAL_DIR}/Src/stm32h7xx_hal_pwr_ex.c
  ${HAL_DIR}/Src/stm32h7xx_hal_rcc.c
  ${HAL_DIR}/Src/stm32h7xx_hal_rcc_ex.c
  ${CMSIS_DEV_DIR}/Source/Templates/system_stm32h7xx.c
  ${CMSIS_DEV_DIR}/Source/Templates/gcc/startup_stm32h723xx.s)
target_include_directories(stm32_hal SYSTEM PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}   # stm32h7xx_hal_conf.h
  ${HAL_DIR}/Inc
  ${CMSIS_DEV_DIR}/Include
  ${CMSIS_CORE_DIR}/Include)
target_compile_definitions(stm32_hal PUBLIC
  STM32H723xx USE_HAL_DRIVER USE_PWR_LDO_SUPPLY)
# HAL_GetTick / HAL_InitTick are provided by support.c (the HAL's are __weak).
target_compile_options(stm32_hal PRIVATE -Wno-unused-parameter)

if(NOT TARGET microros)
  # libmicroros.a built for Cortex-M7 by tools/microros/build_libmicroros.sh (TARGET=stm32).
  set(VCU_MICROROS_DIR "${PROJECT_SOURCE_DIR}/build/microros-stm32/firmware/build"
      CACHE PATH "Directory holding the Cortex-M7 libmicroros.a and include/")
  option(VCU_ENABLE_MICROROS "Link micro-ROS (serial) into the firmware" ON)
  if(VCU_ENABLE_MICROROS)
    if(NOT EXISTS "${VCU_MICROROS_DIR}/libmicroros.a")
      message(FATAL_ERROR "libmicroros.a not found in ${VCU_MICROROS_DIR}; run "
                          "TARGET=stm32 tools/microros/build_libmicroros.sh, or configure "
                          "with -DVCU_ENABLE_MICROROS=OFF")
    endif()
    add_library(microros STATIC IMPORTED)
    set_target_properties(microros PROPERTIES
      IMPORTED_LOCATION "${VCU_MICROROS_DIR}/libmicroros.a")
    target_include_directories(microros SYSTEM INTERFACE "${VCU_MICROROS_DIR}/include")
  endif()
endif()

# ---- TinyUSB (device CDC) ----
set(TUSB_DIR ${PROJECT_SOURCE_DIR}/third_party/tinyusb/src)
add_library(tinyusb STATIC
  ${TUSB_DIR}/tusb.c
  ${TUSB_DIR}/common/tusb_fifo.c
  ${TUSB_DIR}/device/usbd.c
  ${TUSB_DIR}/class/cdc/cdc_device.c
  ${TUSB_DIR}/portable/synopsys/dwc2/dcd_dwc2.c
  ${TUSB_DIR}/portable/synopsys/dwc2/dwc2_common.c)
target_include_directories(tinyusb SYSTEM PUBLIC ${TUSB_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(tinyusb PUBLIC stm32_hal freertos_kernel)
target_compile_options(tinyusb PRIVATE -Wno-unused-parameter)

add_subdirectory(${PROJECT_SOURCE_DIR}/platform_common ${CMAKE_BINARY_DIR}/platform_common)

add_executable(corb_firmware main.cpp board.cpp usb_cdc.cpp support.c)
target_link_libraries(corb_firmware PRIVATE platform_common stm32_hal tinyusb)
target_link_options(corb_firmware PRIVATE
  -T${CMAKE_CURRENT_SOURCE_DIR}/STM32H723ZG_FLASH.ld
  -Wl,--no-warn-rwx-segments
  -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/corb_firmware.map
  -u _printf_float)
set_target_properties(corb_firmware PROPERTIES LINK_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/STM32H723ZG_FLASH.ld SUFFIX .elf)
# Our own sources get strict warnings; support.c includes vendor headers, so leave it out.
set_source_files_properties(main.cpp board.cpp PROPERTIES COMPILE_OPTIONS
  "-Wall;-Wextra;-Wpedantic;-Wconversion;-Wshadow;-Werror")

if(VCU_ENABLE_MICROROS)
  target_link_libraries(corb_firmware PRIVATE microros_bridge)
  target_compile_definitions(corb_firmware PRIVATE VCU_ENABLE_MICROROS=1)
endif()

add_custom_command(TARGET corb_firmware POST_BUILD
  COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:corb_firmware> corb_firmware.bin
  COMMAND ${CMAKE_SIZE} $<TARGET_FILE:corb_firmware>)
