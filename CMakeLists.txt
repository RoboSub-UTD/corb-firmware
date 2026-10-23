cmake_minimum_required(VERSION 3.22)
project(corb_firmware LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(VCU_BUILD_TESTS "Build unit tests" ON)
option(VCU_BUILD_SIM "Build the FreeRTOS POSIX simulator executable" ON)
option(VCU_BUILD_STM32 "Build the STM32H723 firmware (needs cmake/arm-none-eabi-cm7.cmake)" OFF)
option(VCU_SANITIZE "Build with AddressSanitizer and UBSan" OFF)

if(VCU_SANITIZE)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

# Strict warnings, applied to our own targets only (never FreeRTOS / GoogleTest).
function(vcu_set_warnings target)
  target_compile_options(${target} PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror)
endfunction()

add_subdirectory(core)

if(VCU_BUILD_SIM)
  add_subdirectory(platform_sim)
endif()

if(VCU_BUILD_STM32)
  add_subdirectory(platform_stm32)
endif()

if(VCU_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
