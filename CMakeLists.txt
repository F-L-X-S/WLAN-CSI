cmake_minimum_required(VERSION 3.14)
project(WLAN-CSI)

# ---------- Main Application  ----------------
# GoogleTest requires at least C++14
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# external packages
include(FetchContent)
include_directories(/opt/homebrew/opt/liquid-dsp/include) # edit this path to your liquid-dsp installation
link_directories(/opt/homebrew/opt/liquid-dsp/lib)

# internal libraries 
include_directories(include)
add_library(power_trigger_lib include/power_trigger/power_trigger.cc)

# source files
set(SOURCE_FILES
  src/main.cc
)

# executable
add_executable(${PROJECT_NAME} ${SOURCE_FILES})  

# link libraries to executable
target_include_directories(${PROJECT_NAME} PRIVATE ${LIQUID_INCLUDE_DIRS})
target_link_libraries(
  ${PROJECT_NAME} 
  power_trigger_lib
  liquid
  )

# ---------- Testing ----------------
# include googletest-framework
FetchContent_Declare(googletest URL https://github.com/google/googletest/archive/refs/heads/main.zip
)
FetchContent_MakeAvailable(googletest)
enable_testing()

# add test-executable power_trigger
add_executable(power_trigger_test tests/power_trigger_test.cc)

# link libraries to executable
target_link_libraries(
  power_trigger_test
  power_trigger_lib
  GTest::gtest_main
)

# add test-executable correlation
add_executable(correlation_test tests/correlation_test.cc)

# link libraries to executable
target_link_libraries(
  correlation_test
  GTest::gtest_main
)


include(GoogleTest)
gtest_discover_tests(power_trigger_test)
gtest_discover_tests(correlation_test)

