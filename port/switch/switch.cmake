# The Nintendo Switch homebrew build (docs/SWITCH.md), included by port/CMakeLists.txt when the
# devkitPro Switch toolchain is in use. Kazarov's host code builds unchanged on the SDL3 and NVK in
# tools/switch/Containerfile; this file supplies what libnx lacks underneath it and packages the
# executable as an .nro.

set(DC_SWITCH_DIR ${CMAKE_CURRENT_LIST_DIR})

# SDL3 and NVK (Vulkan) from the image, ahead of devkitPro's portlibs.
if(DEFINED ENV{SWITCH_PREFIX})
    list(PREPEND CMAKE_PREFIX_PATH $ENV{SWITCH_PREFIX})
    set(Vulkan_INCLUDE_DIR $ENV{SWITCH_PREFIX}/include CACHE PATH "NVK's Vulkan headers")
    set(Vulkan_LIBRARY $ENV{SWITCH_PREFIX}/lib/libvulkan_nouveau.a CACHE FILEPATH "NVK, linked statically")
    # SDL3 links EGL by name: the image's, from the same Mesa as NVK, not portlibs' Mesa 20.1, whose
    # copies of Mesa's utility code collide with NVK's.
    set(CMAKE_EXE_LINKER_FLAGS "-L$ENV{SWITCH_PREFIX}/lib ${CMAKE_EXE_LINKER_FLAGS}")
endif()

# What newlib and libnx lack of POSIX (switch_compat.h, ucontext.h), ahead of every unit.
include_directories(BEFORE SYSTEM ${DC_SWITCH_DIR}/include)
add_compile_options("SHELL:-include \"${DC_SWITCH_DIR}/include/switch_compat.h\"")

# vulkan: Kazarov's renderer on NVK. gl: port/switch/gl, the OpenGL backend kept in reserve should
# NVK fail on a console; it implements gfx.hpp over Kazarov's display lists and TIM2 decoding.
set(DC_SWITCH_RENDERER vulkan CACHE STRING "The Switch renderer: vulkan (NVK) or gl")
set_property(CACHE DC_SWITCH_RENDERER PROPERTY STRINGS vulkan gl)

# Kazarov's host library with the units that need an operating system's virtual memory replaced by
# Switch ones, and with the GL renderer in place of the Vulkan one when it is picked.
function(dc_switch_host target)
    set_source_files_properties(${KZ_ROOT}/port/platform/memory.cpp PROPERTIES HEADER_FILE_ONLY ON)
    target_sources(${target} PRIVATE ${DC_SWITCH_DIR}/memory.cpp ${DC_SWITCH_DIR}/ucontext.cpp ${DC_SWITCH_DIR}/nxlink.cpp
        ${DC_SWITCH_DIR}/clocks.cpp)
    if(DC_SWITCH_RENDERER STREQUAL "gl")
        file(GLOB vulkan_units ${KZ_ROOT}/port/gfx/*.cpp)
        list(FILTER vulkan_units EXCLUDE REGEX "/(displaylist|tim2)\\.cpp$")
        set_source_files_properties(${vulkan_units} PROPERTIES HEADER_FILE_ONLY ON)
        file(GLOB gl_units CONFIGURE_DEPENDS ${DC_SWITCH_DIR}/gl/*.cpp)
        target_sources(${target} PRIVATE ${gl_units})
        target_include_directories(${target} PRIVATE ${KZ_ROOT}/port/gfx)
    elseif(NOT DC_SWITCH_RENDERER STREQUAL "vulkan")
        message(FATAL_ERROR "DC_SWITCH_RENDERER is vulkan or gl, not ${DC_SWITCH_RENDERER}")
    endif()
endfunction()

# darkcloud.nro: the executable with its title, author and icon for the Homebrew Menu.
function(dc_switch_nro target)
    nx_generate_nacp(${CMAKE_BINARY_DIR}/darkcloud.nacp NAME "Chronicle" AUTHOR "Chronicle contributors"
        VERSION "0.1.0")
    nx_create_nro(${target} NACP ${CMAKE_BINARY_DIR}/darkcloud.nacp ICON ${DC_SWITCH_DIR}/icon.jpg
        OUTPUT ${CMAKE_BINARY_DIR}/darkcloud.nro)
endfunction()
