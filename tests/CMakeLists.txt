include(FetchContent)
FetchContent_Declare(googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.15.2
  GIT_SHALLOW    TRUE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(vcu_tests
  test_command_gate.cpp
  test_state_machine.cpp
  test_vcu.cpp)
target_link_libraries(vcu_tests PRIVATE vcu_core GTest::gtest_main)
vcu_set_warnings(vcu_tests)

include(GoogleTest)
gtest_discover_tests(vcu_tests)
