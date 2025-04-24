cmake_minimum_required(VERSION 3.14)
project(WLAN-CSI)

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
add_library(auto_corr_lib include/correlation/auto_corr.cc)
target_link_libraries(auto_corr_lib liquid)
add_library(signal_generator_lib include/signal_generator/signal_generator.cc)
target_link_libraries(signal_generator_lib liquid)


# ---------- Main Application  ----------------
# source files
set(MAIN_SRC_FILES
  src/main.cc
)

# executables
add_executable(${PROJECT_NAME} ${MAIN_SRC_FILES})  

# link libraries to executable
target_link_libraries(
  ${PROJECT_NAME} 
  power_trigger_lib
  auto_corr_lib
  liquid
  )

# ---------- Sandbox ----------------
# Define source files
set(SANDBOX_SRC_FILES
  sandbox/example_symsync_get_tau.cc
  sandbox/example_auto_correlation.cc
)

# Iterate over each file in SOURCE_FILES
foreach(SOURCE_FILE ${SANDBOX_SRC_FILES})
  # Extract the filename without the directory and extension
  get_filename_component(EXECUTABLE_NAME ${SOURCE_FILE} NAME_WE)

  # Create an executable for each source file
  add_executable(${EXECUTABLE_NAME} ${SOURCE_FILE})

  # Link libraries to the executable
  target_link_libraries(${EXECUTABLE_NAME}
    power_trigger_lib
    auto_corr_lib
    signal_generator_lib
    liquid
  )

  # Include directories for each executable
  target_include_directories(${EXECUTABLE_NAME} PRIVATE ${LIQUID_INCLUDE_DIRS} include)
endforeach()

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
  auto_corr_lib
  GTest::gtest_main
)


include(GoogleTest)
gtest_discover_tests(power_trigger_test)
gtest_discover_tests(correlation_test)

