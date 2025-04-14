cmake_minimum_required(VERSION 3.14)
project(WLAN-CSI)

include_directories(include)

# GoogleTest requires at least C++14
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# libraries 
add_library(power_trigger_lib include/power_trigger/power_trigger.cc)

# executable
add_executable(main src/main.cc)  

# link libraries to executable
target_link_libraries(
  main 
  power_trigger_lib
  )

# include googletest-framework
include(FetchContent)
FetchContent_Declare(googletest URL https://github.com/google/googletest/archive/refs/heads/main.zip
)
FetchContent_MakeAvailable(googletest)
enable_testing()

# add test-executables
add_executable(power_trigger_test tests/power_trigger_test.cc)

# link libraries to executable
target_link_libraries(
  power_trigger_test
  power_trigger_lib
  GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(power_trigger_test)

