add_library(vcu_core STATIC
  src/command_gate.cpp
  src/state_machine.cpp
  src/vcu.cpp)
target_include_directories(vcu_core PUBLIC include)
target_compile_features(vcu_core PUBLIC cxx_std_17)
vcu_set_warnings(vcu_core)
