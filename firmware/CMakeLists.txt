# DensePose-ESP32 Firmware
# Top-level CMakeLists.txt for ESP-IDF project

# Minimum CMake version required by ESP-IDF
cmake_minimum_required(VERSION 3.16)

# Include ESP-IDF build system
# This must come before project() to set up the toolchain
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Define the project
project(densepose_esp32)

# Include the test component
# This makes tests discoverable by the build system
set(EXTRA_COMPONENT_DIRS "test")
