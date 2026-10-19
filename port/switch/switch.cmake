# The Nintendo Switch homebrew build (docs/SWITCH.md), included by port/CMakeLists.txt when the
# devkitPro Switch toolchain is in use. Kazarov's host code builds unchanged on the SDL3 and Mesa in
# the toolchain images; this file supplies what libnx lacks underneath it and packages the
# executable as an .nro. DC_RENDERER (port/CMakeLists.txt) picks Kazarov's Vulkan renderer
# on NVK (tools/switch/Containerfile) or port/gl on devkitPro's Mesa (tools/switch/Containerfile.gl).

set(DC_SWITCH_DIR ${CMAKE_CURRENT_LIST_DIR})

if(DEFINED ENV{SWITCH_PREFIX})
    list(PREPEND CMAKE_PREFIX_PATH $ENV{SWITCH_PREFIX})
    if(DC_RENDERER STREQUAL "gl")
        # tools/switch/Containerfile.gl: SDL3 over devkitPro's Mesa. Nothing links Vulkan; the display
        # lists' context still names its types, so the target is the headers alone.
        add_library(Vulkan::Vulkan INTERFACE IMPORTED)
        set_target_properties(Vulkan::Vulkan PROPERTIES INTERFACE_INCLUDE_DIRECTORIES $ENV{SWITCH_PREFIX}/include)
    else()
        # tools/switch/Containerfile: SDL3 and NVK (Vulkan), ahead of devkitPro's portlibs.
        set(Vulkan_INCLUDE_DIR $ENV{SWITCH_PREFIX}/include CACHE PATH "NVK's Vulkan headers")
        set(Vulkan_LIBRARY $ENV{SWITCH_PREFIX}/lib/libvulkan_nouveau.a CACHE FILEPATH "NVK, linked statically")
        # SDL3 links EGL by name: the image's, from the same Mesa as NVK, not portlibs' Mesa 20.1, whose
        # copies of Mesa's utility code collide with NVK's.
        set(CMAKE_EXE_LINKER_FLAGS "-L$ENV{SWITCH_PREFIX}/lib ${CMAKE_EXE_LINKER_FLAGS}")
    endif()
endif()

# What newlib and libnx lack of POSIX (switch_compat.h, ucontext.h), ahead of every unit.
include_directories(BEFORE SYSTEM ${DC_SWITCH_DIR}/include)
add_compile_options("SHELL:-include \"${DC_SWITCH_DIR}/include/switch_compat.h\"")
# Plain char is signed on the PS2 (and x64) and unsigned on AArch64; ps2/include/types.h makes s8
# a char. Unsigned, a sound table's -128 terminator reads as 128 and SndInitSeTable writes past it.
add_compile_options(-fsigned-char)

# Kazarov's host library with the units that need an operating system's virtual memory replaced by
# Switch ones.
function(dc_switch_host target)
    set_source_files_properties(${KZ_ROOT}/port/platform/memory.cpp PROPERTIES HEADER_FILE_ONLY ON)
    target_sources(${target} PRIVATE ${DC_SWITCH_DIR}/memory.cpp ${DC_SWITCH_DIR}/ucontext.cpp ${DC_SWITCH_DIR}/nxlink.cpp
        ${DC_SWITCH_DIR}/clocks.cpp)
endfunction()

# darkcloud.nro: the executable with its title, author and icon for the Homebrew Menu.
function(dc_switch_nro target)
    nx_generate_nacp(${CMAKE_BINARY_DIR}/darkcloud.nacp NAME "Chronicle" AUTHOR "Chronicle contributors"
        VERSION "0.1.0")
    nx_create_nro(${target} NACP ${CMAKE_BINARY_DIR}/darkcloud.nacp ICON ${DC_SWITCH_DIR}/icon.jpg
        OUTPUT ${CMAKE_BINARY_DIR}/darkcloud.nro)
endfunction()
