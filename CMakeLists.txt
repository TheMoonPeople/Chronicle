cmake_minimum_required(VERSION 3.20)
project(dcdecomp NONE)

# PLATFORM picks what is built: PS2 builds and verifies the retail images
# (ps2/CMakeLists.txt), PC builds the x64 port (port/CMakeLists.txt), SWITCH builds
# the port as a Nintendo Switch homebrew .nro with devkitPro (docs/SWITCH.md).
set(PLATFORM PS2 CACHE STRING "Platform to build: PS2, PC or SWITCH")
set_property(CACHE PLATFORM PROPERTY STRINGS PS2 PC SWITCH)
if(PLATFORM STREQUAL "PS2")
    add_subdirectory(ps2)
elseif(PLATFORM STREQUAL "PC")
    # Here rather than in port so ctest finds the tests from the top of the build tree.
    enable_testing()
    add_subdirectory(port)
elseif(PLATFORM STREQUAL "SWITCH")
    if(NOT NINTENDO_SWITCH)
        message(FATAL_ERROR "PLATFORM SWITCH needs devkitPro's Switch.cmake toolchain file (the switch preset)")
    endif()
    add_subdirectory(port)
else()
    message(FATAL_ERROR "PLATFORM must be PS2, PC or SWITCH, not ${PLATFORM}")
endif()
