# Host (laptop) toolchain for building libmicroros.a for the simulator.
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_C_COMPILER gcc)
set(CMAKE_CXX_COMPILER g++)
set(CMAKE_C_FLAGS_INIT "-std=c11 -fPIC" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_INIT "-std=c++17 -fPIC" CACHE STRING "" FORCE)
