# The Nintendo Switch homebrew build, included by port/CMakeLists.txt with devkitPro's toolchain.
# This file supplies the POSIX interfaces newlib lacks and packages the executable as an .nro.

set(DC_SWITCH_DIR ${CMAKE_CURRENT_LIST_DIR})
# Horizon lacks POSIX mmap; simdjson's optional mmap wrapper is unused by fastgltf.
set(CMAKE_PROJECT_simdjson_INCLUDE_BEFORE "${CMAKE_CURRENT_LIST_DIR}/patch_simdjson.cmake")

if(DEFINED ENV{SWITCH_PREFIX})
    list(PREPEND CMAKE_PREFIX_PATH $ENV{SWITCH_PREFIX})
    set(Vulkan_INCLUDE_DIR $ENV{SWITCH_PREFIX}/include CACHE PATH "NVK's Vulkan headers")
    set(Vulkan_LIBRARY $ENV{SWITCH_PREFIX}/lib/libvulkan_nouveau.a CACHE FILEPATH "NVK, linked statically")
    # SDL3 and NVK use the same Mesa libraries, ahead of devkitPro's older portlibs.
    set(CMAKE_EXE_LINKER_FLAGS "-L$ENV{SWITCH_PREFIX}/lib ${CMAKE_EXE_LINKER_FLAGS}")
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
    set_source_files_properties(${KZ_PORT_ROOT}/platform/memory.cpp PROPERTIES HEADER_FILE_ONLY ON)
    target_sources(${target} PRIVATE ${DC_SWITCH_DIR}/src/memory.cpp ${DC_SWITCH_DIR}/src/ucontext.cpp
        ${DC_SWITCH_DIR}/src/nxlink.cpp ${DC_SWITCH_DIR}/src/clocks.cpp)
endfunction()

function(dc_switch_port target)
    target_sources(${target} PRIVATE ${DC_SWITCH_DIR}/src/assert.cpp ${DC_SWITCH_DIR}/src/discord.cpp
        ${DC_SWITCH_DIR}/src/firstrun.cpp ${DC_SWITCH_DIR}/src/main.cpp)
endfunction()

# darkcloud.nro: the executable with its title, author and icon for the Homebrew Menu.
function(dc_switch_nro target)
    nx_generate_nacp(${CMAKE_BINARY_DIR}/darkcloud.nacp NAME "Chronicle" AUTHOR "Chronicle contributors"
        VERSION "0.1.0")
    nx_create_nro(${target} NACP ${CMAKE_BINARY_DIR}/darkcloud.nacp ICON ${DC_SWITCH_DIR}/icon.jpg
        OUTPUT ${CMAKE_BINARY_DIR}/darkcloud.nro)
endfunction()
