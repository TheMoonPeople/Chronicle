cmake_minimum_required(VERSION 3.20)
project(dcdecomp NONE)

# Allows local candidate builds against an isolated Kazarov checkout.
set(KAZAROV_SOURCE_DIR "${CMAKE_SOURCE_DIR}/tools/kazarov" CACHE PATH "Kazarov source tree")
get_filename_component(KAZAROV_SOURCE_DIR "${KAZAROV_SOURCE_DIR}" ABSOLUTE BASE_DIR "${CMAKE_SOURCE_DIR}")

# PLATFORM picks what is built: PS2 builds and verifies the retail images
# (ps2/CMakeLists.txt), PC builds the x64 port (port/CMakeLists.txt).
set(PLATFORM PS2 CACHE STRING "Platform to build: PS2, PC, Switch or Android")
set_property(CACHE PLATFORM PROPERTY STRINGS PS2 PC SWITCH ANDROID)
if(PLATFORM STREQUAL "PS2")
    add_subdirectory(ps2)
elseif(PLATFORM STREQUAL "PC")
    # Here rather than in port so ctest finds the tests from the top of the build tree.
    enable_testing()
    add_subdirectory(port)
elseif(PLATFORM STREQUAL "SWITCH")
    if(NOT NINTENDO_SWITCH)
        message(FATAL_ERROR "PLATFORM SWITCH requires devkitPro's Switch.cmake toolchain")
    endif()
    add_subdirectory(port)
elseif(PLATFORM STREQUAL "ANDROID")
    add_subdirectory(port)
else()
    message(FATAL_ERROR "PLATFORM must be PS2, PC, SWITCH or ANDROID, not ${PLATFORM}")
endif()
