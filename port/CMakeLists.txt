# The x64 port: the game's code built with clang as C++26 for the host.
#
# ps2/src holds only the game's code as the PS2 build compiles it. Its units are
# compiled as they are and merged into one relocatable object whose definitions
# are all made weak. Units in port/src are compiled normally, so any function or
# variable defined there takes precedence at link time and the ps2/src
# definition is discarded. That is how the PS2-only parts are replaced: the SDK,
# the Metrowerks runtime, the functions that touch the hardware directly and
# those written in assembly.
#
# The port is always the PAL release.

if(NOT CMAKE_CXX_COMPILER)
    set(CMAKE_CXX_COMPILER clang++)
endif()
enable_language(CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# darkcloud is position-independent (DC_LINK_OPTIONS below).
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# The LLVM tools are looked up beside the compiler first: Homebrew's llvm is
# keg-only and apt.llvm.org's unversioned names live in /usr/lib/llvm-N/bin.
get_filename_component(compiler_dir ${CMAKE_CXX_COMPILER} DIRECTORY)
file(REAL_PATH ${CMAKE_CXX_COMPILER} compiler_path)
get_filename_component(compiler_real_dir ${compiler_path} DIRECTORY)
set(LLVM_TOOL_HINTS ${compiler_dir} ${compiler_real_dir})
# llvm-objcopy, not GNU objcopy: its --weaken leaves undefined references
# strong, so a missing replacement is still a link error. It handles Mach-O
# too (LLVM 20 sets N_WEAK_DEF, byte for byte what tools/weaken writes).
find_program(OBJCOPY NAMES llvm-objcopy llvm-objcopy-19 HINTS ${LLVM_TOOL_HINTS} REQUIRED)
find_program(LLVM_NM NAMES llvm-nm HINTS ${LLVM_TOOL_HINTS} REQUIRED)
find_program(LLVM_OBJDUMP NAMES llvm-objdump HINTS ${LLVM_TOOL_HINTS} REQUIRED)
find_program(LLVM_READOBJ NAMES llvm-readobj HINTS ${LLVM_TOOL_HINTS} REQUIRED)
find_program(LLVM_LIPO NAMES llvm-lipo lipo HINTS ${LLVM_TOOL_HINTS} REQUIRED)
if(APPLE)
    # ld64.lld has no -r ("not yet implemented" through LLVM 20); Apple's ld does,
    # and it is the linker clang drives for the final link too.
    find_program(LD_RELOCATABLE NAMES ld PATHS /usr/bin NO_DEFAULT_PATH REQUIRED)
    set(DC_MACHO_WEAKEN objcopy CACHE STRING "What weakens the merged Mach-O object: objcopy or tool")
    set_property(CACHE DC_MACHO_WEAKEN PROPERTY STRINGS objcopy tool)
else()
    find_program(LD_RELOCATABLE NAMES ld.lld HINTS ${LLVM_TOOL_HINTS} REQUIRED)
endif()
option(DC_INTERPOSITION_CHECK "Check that ps2/src's calls to replaced functions go through the linker" ON)
if(DC_INTERPOSITION_CHECK)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
endif()
# SDL3 owns the window and input; Vulkan 1.4 renders.
find_package(SDL3 REQUIRED CONFIG)
find_package(Vulkan 1.4 REQUIRED)
# nlohmann/json reads config.json: the installed package where there is one, fetched otherwise.
include(FetchContent)
FetchContent_Declare(nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
    URL_HASH SHA256=42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa
    FIND_PACKAGE_ARGS CONFIG)
FetchContent_MakeAvailable(nlohmann_json)

set(PS2_DIR ${CMAKE_SOURCE_DIR}/ps2/src)
set(PORT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
file(GLOB_RECURSE PS2_SOURCES CONFIGURE_DEPENDS RELATIVE ${PS2_DIR} ${PS2_DIR}/*.cpp)
file(GLOB_RECURSE PORT_SOURCES CONFIGURE_DEPENDS ${PORT_DIR}/*.cpp)
list(FILTER PORT_SOURCES EXCLUDE REGEX "/tests/")
# tools/mwccgap compiles a temporary beside the source it came from.
list(FILTER PS2_SOURCES EXCLUDE REGEX "(^|/)tmp[^/]*$")

set(PS2_UNITS "")
# port/include/stubs/<unit>.hpp, if it exists, is included ahead of that unit
# of ps2/src: it declares what the port defines in place of code PORT leaves
# out, and supplies or renames what the unit takes from MWCC alone.
foreach(unit IN LISTS PS2_SOURCES)
    list(APPEND PS2_UNITS ${PS2_DIR}/${unit})
    string(REGEX REPLACE "\\.cpp$" ".hpp" header ${CMAKE_SOURCE_DIR}/port/include/stubs/${unit})
    if(EXISTS ${header})
        set_property(SOURCE ${PS2_DIR}/${unit} APPEND PROPERTY COMPILE_OPTIONS -include ${header})
        set_property(SOURCE ${PS2_DIR}/${unit} APPEND PROPERTY OBJECT_DEPENDS ${header})
    endif()
endforeach()

# What both halves are compiled with. The game was written for MWCC, which
# accepts constructs clang rejects by default: pointers truncated to 32-bit
# integers, narrowing in braced initialisers and the register keyword.
add_library(dc_options INTERFACE)
# PORT leaves out the functions ps2/src writes in MIPS assembly.
target_compile_definitions(dc_options INTERFACE PORT PAL)
target_include_directories(dc_options INTERFACE
    ${CMAKE_SOURCE_DIR}/port/include ${CMAKE_SOURCE_DIR}/ps2/include ${CMAKE_SOURCE_DIR}/ps2/include/sce)
target_compile_options(dc_options INTERFACE
    "SHELL:-include ${CMAKE_SOURCE_DIR}/port/include/port.h"
    -fms-extensions -ffunction-sections -fdata-sections
    -Wno-register -Wno-c++11-narrowing -Wno-return-mismatch)
# arm64 clang fuses a*b+c into one rounding by default; x86-64 has no FMA in its
# baseline. Off everywhere so the game's arithmetic rounds the same on both.
target_compile_options(dc_options INTERFACE -ffp-contract=off)

# ps2/src: semantic interposition keeps clang from inlining or folding a call
# to a function port/src may replace, so every such call reaches the linker.
# The driver only passes -fsemantic-interposition for ELF. Mach-O definitions
# are not dso_local, so the flag given to cc1 makes LLVM treat them as
# interposable just the same; without it -fno-inline-functions alone still
# lets IPSCCP and attribute inference delete calls to functions that return a
# constant or do nothing. ps2_interposition_check verifies the result.
add_library(dc_ps2 OBJECT ${PS2_UNITS})
target_link_libraries(dc_ps2 PRIVATE dc_options)
if(APPLE)
    target_compile_options(dc_ps2 PRIVATE "SHELL:-Xclang -fsemantic-interposition" -fno-inline-functions)
    set_property(SOURCE ${PS2_DIR}/title/op_a.cpp ${PS2_DIR}/title/op_b.cpp ${PS2_DIR}/title/op_c.cpp
        ${PS2_DIR}/title/rushmovi.cpp
        APPEND PROPERTY COMPILE_OPTIONS -fno-zero-initialized-in-bss)
else()
    target_compile_options(dc_ps2 PRIVATE -fPIC -fsemantic-interposition)
endif()
# Retail's code reads locals it never set and falls off the end of functions
# that return a value, and ps2/src keeps both because the match depends on
# them. MWCC gave each a defined if arbitrary result; these flags give the port
# a defined one too, along with the type punning and wraparound the code assumes.
target_compile_options(dc_ps2 PRIVATE -ftrivial-auto-var-init=zero -fno-strict-return
    -fno-strict-aliasing -fwrapv)
# What is left on warns about behaviour the port may still need to answer for:
# uninitialised reads, missing returns, format strings, overlapping copies and
# the like. What is turned off describes how MWCC-era source is spelled -- string
# literals as char *, MWCC's pragmas, 32-bit pointer casts, the unused names
# and expressions matching leaves behind -- which ps2/src cannot change without
# changing what MWCC compiles.
target_compile_options(dc_ps2 PRIVATE -Wall -Wno-writable-strings -Wno-unknown-pragmas
    -Wno-mismatched-tags -Wno-missing-braces -Wno-char-subscripts -Wno-overloaded-virtual
    -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast -Wno-int-to-void-pointer-cast
    -Wno-void-pointer-to-int-cast -Wno-unused-variable -Wno-unused-but-set-variable
    -Wno-unused-local-typedef -Wno-unused-label -Wno-unused-function -Wno-unused-value
    -Wno-unused-const-variable -Wno-undefined-internal -Wno-self-assign -Wno-deprecated-volatile)

# Names the PS2 build tells apart per object (ps2/config/*/object_fixups.json) are
# renamed by port/include/stubs, so no two units define the same strong name and
# the merge allows none: Apple's ld -r has no --allow-multiple-definition, and a
# duplicate fails on both platforms alike.
add_subdirectory(${CMAKE_SOURCE_DIR}/tools/weaken ${CMAKE_BINARY_DIR}/tools/weaken)
set(PS2_OBJECT ${CMAKE_BINARY_DIR}/dc_ps2.o)
if(APPLE)
    if(DC_MACHO_WEAKEN STREQUAL "tool")
        set(weaken_command $<TARGET_FILE:weaken> ${PS2_OBJECT})
        set(weaken_depends weaken)
    else()
        set(weaken_command ${OBJCOPY} --weaken ${PS2_OBJECT})
        set(weaken_depends "")
    endif()
    add_custom_command(OUTPUT ${PS2_OBJECT}
        COMMAND ${LD_RELOCATABLE} -r -keep_private_externs -o ${PS2_OBJECT} $<TARGET_OBJECTS:dc_ps2>
        COMMAND ${weaken_command}
        DEPENDS $<TARGET_OBJECTS:dc_ps2> ${weaken_depends}
        COMMAND_EXPAND_LISTS
        COMMENT "Merging ps2/src into one weak object")
else()
    add_custom_command(OUTPUT ${PS2_OBJECT}
        COMMAND ${LD_RELOCATABLE} -r -o ${PS2_OBJECT} $<TARGET_OBJECTS:dc_ps2>
        COMMAND ${OBJCOPY} --weaken ${PS2_OBJECT}
        DEPENDS $<TARGET_OBJECTS:dc_ps2>
        COMMAND_EXPAND_LISTS
        COMMENT "Merging ps2/src into one weak object")
endif()

# Everything in port/src except main.cpp is an object library rather than an
# archive: an archive member only enters the link when something references
# it, so a strong definition sitting in one would never displace the weak
# ps2/src copy. Objects named on the link line always do.
set(PORT_LIB_SOURCES ${PORT_SOURCES})
list(FILTER PORT_LIB_SOURCES EXCLUDE REGEX "/tests/|/main\\.cpp$")
# The host side (platform, gfx, audio) never sees a game header, so it is built
# without dc_options: no port.h ahead of each unit and no game include paths.
set(HOST_SOURCES ${PORT_LIB_SOURCES})
list(FILTER HOST_SOURCES INCLUDE REGEX "/port/src/(platform|gfx|audio)/")
list(FILTER PORT_LIB_SOURCES EXCLUDE REGEX "/port/src/(platform|gfx|audio)/")

# Every shaders/*.vert|frag under port/src/gfx becomes a header holding its
# SPIR-V words as a const uint32_t array named after the file (draw.frag ->
# draw_frag), in ${SHADER_HEADER_DIR}/shaders/<file>.spv.hpp.
find_program(GLSLANG_VALIDATOR NAMES glslangValidator REQUIRED)
set(SHADER_DIR ${PORT_DIR}/gfx/shaders)
set(SHADER_HEADER_DIR ${CMAKE_BINARY_DIR}/generated)
file(GLOB SHADER_SOURCES CONFIGURE_DEPENDS ${SHADER_DIR}/*.vert ${SHADER_DIR}/*.frag)
set(SHADER_HEADERS "")
foreach(shader IN LISTS SHADER_SOURCES)
    get_filename_component(shader_name ${shader} NAME)
    string(REPLACE "." "_" shader_symbol ${shader_name})
    set(header ${SHADER_HEADER_DIR}/shaders/${shader_name}.spv.hpp)
    add_custom_command(OUTPUT ${header}
        COMMAND ${GLSLANG_VALIDATOR} -V --target-env vulkan1.4 --quiet --vn ${shader_symbol}
            --depfile ${header}.d -o ${header} ${shader}
        DEPENDS ${shader}
        DEPFILE ${header}.d
        COMMENT "Compiling ${shader_name} to SPIR-V")
    list(APPEND SHADER_HEADERS ${header})
endforeach()

add_library(dc_host OBJECT ${HOST_SOURCES} ${SHADER_HEADERS})
target_include_directories(dc_host PRIVATE ${PORT_DIR} ${SHADER_HEADER_DIR})
target_link_libraries(dc_host PRIVATE SDL3::SDL3 Vulkan::Vulkan nlohmann_json::nlohmann_json)
target_compile_options(dc_host PRIVATE -Wall -ffunction-sections -fdata-sections)

add_library(dc_port OBJECT ${PORT_LIB_SOURCES})
target_link_libraries(dc_port PRIVATE dc_options SDL3::SDL3 Vulkan::Vulkan)
# Replacement units reach the host side as "gfx/gfx.hpp", "platform/window.hpp".
target_include_directories(dc_port PRIVATE ${PORT_DIR})
# The game's headers trip a few warnings that only describe how MWCC's source
# was written, not a problem in the port.
target_compile_options(dc_port PRIVATE -Wall -Wno-mismatched-tags -Wno-unused-label
    -Wno-overloaded-virtual -Wno-unused-function)

# ps2/src calls a function port/src replaces only through a relocation against
# its name; this fails the build when the compiler bound such a call inside
# dc_ps2.o, or when dc_ps2.o still holds a strong definition.
if(DC_INTERPOSITION_CHECK)
    set(interposition_stamp ${CMAKE_BINARY_DIR}/ps2_interposition_check.stamp)
    set(interposition_script ${CMAKE_SOURCE_DIR}/tools/weaken/interposition_check.py)
    add_custom_command(OUTPUT ${interposition_stamp}
        COMMAND ${Python3_EXECUTABLE} ${interposition_script} --nm ${LLVM_NM} --objdump ${LLVM_OBJDUMP}
            --stamp ${interposition_stamp} ${PS2_OBJECT} $<TARGET_OBJECTS:dc_port>
        DEPENDS ${PS2_OBJECT} $<TARGET_OBJECTS:dc_port> ${interposition_script}
        COMMAND_EXPAND_LISTS
        COMMENT "Checking that ps2/src reaches port/src's replacements through the linker")
    add_custom_target(ps2_interposition_check ALL DEPENDS ${interposition_stamp})
endif()

# Shared by the game and its tests: the port's objects, the weakened game and
# the link flags they need. The executable is position-independent on every
# platform: the game's casts of pointers to 32-bit integers are all widened in
# port/src (docs/port/truncations.md), so nothing has to sit below 4 GiB.
# draw_rect and WorkBuffer__2 are second names for existing storage, as the PS2
# build gives them through object_fixups.json. port/src/linknames.cpp defines
# the names that are not aliases, EditGaijiTbl among them. ItemPutListTbl12_bytes
# is not one: port/include/stubs/dngstatusdata.hpp turns GetItem's read through
# it into a WeaponList lookup.
if(APPLE)
    set(DC_LINK_OPTIONS -Wl,-dead_strip -Wl,-alias,_draw_rect_store,_draw_rect
        -Wl,-alias,_WorkBuffer,_WorkBuffer__2)
else()
    set(DC_LINK_OPTIONS -fuse-ld=lld -pie -Wl,-z,separate-code -Wl,--gc-sections -Wl,--error-limit=0
        -Wl,--defsym=draw_rect=draw_rect_store -Wl,--defsym=WorkBuffer__2=WorkBuffer)
endif()

function(dc_link_game target)
    target_sources(${target} PRIVATE $<TARGET_OBJECTS:dc_port> $<TARGET_OBJECTS:dc_host> ${PS2_OBJECT})
    target_link_libraries(${target} PRIVATE dc_options SDL3::SDL3 Vulkan::Vulkan)
    target_include_directories(${target} PRIVATE ${PORT_DIR})
    target_compile_options(${target} PRIVATE -Wall -Wno-mismatched-tags -Wno-unused-label
        -Wno-overloaded-virtual -Wno-unused-function)
    target_link_options(${target} PRIVATE ${DC_LINK_OPTIONS})
endfunction()

add_executable(darkcloud ${PORT_DIR}/main.cpp)
set_target_properties(darkcloud PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
dc_link_game(darkcloud)

include(GNUInstallDirs)
install(TARGETS darkcloud RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# The tests use an installed GoogleTest, or fetch one at configure time; a build with neither turns them off.
option(DC_BUILD_TESTS "Build darkcloud_tests" ON)
if(DC_BUILD_TESTS)
    enable_testing()
    add_subdirectory(src/tests)
endif()
add_subdirectory(${CMAKE_SOURCE_DIR}/tools/dcdata ${CMAKE_BINARY_DIR}/tools/dcdata)
