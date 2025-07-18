cmake_minimum_required(VERSION 3.14)
project(WLAN-CSI)

# GoogleTest requires at least C++14
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------- external packages ----------------
include(FetchContent)

# Liquid-DSP
set(liquid_patch git apply ${CMAKE_CURRENT_SOURCE_DIR}/patch/liquid.patch)

FetchContent_Declare(
  liquid
  GIT_REPOSITORY https://github.com/jgaeddert/liquid-dsp
  GIT_TAG        v1.7.0
  PATCH_COMMAND ${liquid_patch}
)

FetchContent_MakeAvailable(liquid)
include_directories(${liquid_SOURCE_DIR}/include)
link_directories(${liquid_SOURCE_DIR}/lib)


# ZeroMQ
# libzmq (remove for using local zmq installation)
FetchContent_Declare(
  libzmq
  GIT_REPOSITORY https://github.com/zeromq/libzmq.git
  GIT_TAG v4.3.5  
)
FetchContent_MakeAvailable(libzmq)

# ZeroMQ
# cppzmq (C++ Header-Wrapper)
FetchContent_Declare(
  cppzmq
  GIT_REPOSITORY https://github.com/zeromq/cppzmq.git
  GIT_TAG v4.10.0
)
FetchContent_MakeAvailable(cppzmq)
target_include_directories(cppzmq INTERFACE ${cppzmq_SOURCE_DIR})

# Find UHD
find_package(UHD REQUIRED)

# Find Boost
find_package(Boost REQUIRED COMPONENTS program_options thread)

# Include-Paths
include_directories(${Boost_INCLUDE_DIRS})
include_directories(${UHD_INCLUDE_DIRS})

# ---------- internal libraries ----------------
include_directories(include)

# power_trigger
add_library(power_trigger_lib include/power_trigger/power_trigger.cc)

# auto_corr
add_library(auto_corr_lib include/correlation/auto_corr.cc)
target_link_libraries(auto_corr_lib liquid)

# signal_generator
add_library(signal_generator_lib include/signal_generator/signal_generator.cc)
target_link_libraries(signal_generator_lib liquid)

# matlab_export
add_library(matlab_export_lib include/matlab_export/matlab_export.cc)

# zmq_socket
add_library(zmq_socket include/zmq_socket/zmq_socket.cc)
target_link_libraries(zmq_socket cppzmq)

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
  zmq_socket
  ${UHD_LIBRARIES}
  ${Boost_LIBRARIES}
  )

# ---------- Sandbox ----------------
# Define source files
set(SANDBOX_SRC_FILES
  sandbox/example_symsync_get_tau.cc
  sandbox/example_auto_correlation.cc
  sandbox/example_qdsync.cc
  sandbox/example_ofdmframesync.cc
  sandbox/example_ofdm_multisync.cc
  sandbox/example_music.cc
  sandbox/example_usrp.cc
  sandbox/example_usrp_music.cc
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
    matlab_export_lib
    liquid
    zmq_socket
    ${UHD_LIBRARIES}
    ${Boost_LIBRARIES}
  )

  # Include directories for each executable
  #target_include_directories(${EXECUTABLE_NAME} PRIVATE ${liquid_SOURCE_DIR}/include)

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

