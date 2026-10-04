# The x64 port: the game's code built with clang as C++26 for the host.
#
# ps2/src holds only the game's code as the PS2 build compiles it. A definition
# port/src tags PC_OVERRIDE replaces the one ps2/src gives the same name: the
# port compiles a copy of each unit without those definitions
# (scripts/port/pc_override.py) and links it with port/src. That is how the
# PS2-only parts are replaced: the SDK, the Metrowerks runtime, the functions
# that touch the hardware directly and those written in assembly.
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

# Python runs scripts/port/pc_override.py.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
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
# What both halves are compiled with (dc_options below). PORT leaves out the
# functions ps2/src writes in MIPS assembly.
set(PORT_DEFINITIONS PORT PAL)
list(TRANSFORM PORT_DEFINITIONS PREPEND --define= OUTPUT_VARIABLE pc_override_defines)
# The names port/src's PC_OVERRIDE definitions link under. The list is rewritten
# only when it changes, so other edits to port/src rebuild nothing of ps2/src.
# PC_OVERRIDE_SOURCES changes when a unit or a stub header comes or goes, which no
# timestamp shows.
set(PC_OVERRIDE_SCRIPT ${CMAKE_SOURCE_DIR}/scripts/port/pc_override.py)
set(PC_OVERRIDES ${CMAKE_BINARY_DIR}/pc_overrides.txt)
set(PC_OVERRIDE_SOURCES ${CMAKE_BINARY_DIR}/pc_override_sources.txt)
file(GLOB_RECURSE PC_OVERRIDE_STUBS CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/port/include/stubs/*.hpp)
string(JOIN "\n" pc_override_sources ${PORT_SOURCES} ${PS2_SOURCES} ${PC_OVERRIDE_STUBS})
file(CONFIGURE OUTPUT ${PC_OVERRIDE_SOURCES} CONTENT "${pc_override_sources}\n" @ONLY)
add_custom_command(OUTPUT ${PC_OVERRIDES}
    COMMAND ${Python3_EXECUTABLE} ${PC_OVERRIDE_SCRIPT} list ${PORT_DIR} -o ${PC_OVERRIDES}
    DEPENDS ${PORT_SOURCES} ${PC_OVERRIDE_SOURCES} ${PC_OVERRIDE_SCRIPT}
    COMMENT "Listing port/src's PC_OVERRIDE definitions"
    VERBATIM)
# What is compiled of a unit is its copy without the definitions of those names.
# port/include/stubs/<unit>.hpp, if it exists, is included ahead of that unit
# of ps2/src: it declares what the port defines in place of code PORT leaves
# out, and supplies or renames what the unit takes from MWCC alone.
foreach(unit IN LISTS PS2_SOURCES)
    set(copy ${CMAKE_BINARY_DIR}/ps2_src/${unit})
    string(REGEX REPLACE "\\.cpp$" ".hpp" header ${CMAKE_SOURCE_DIR}/port/include/stubs/${unit})
    if(EXISTS ${header})
        set(stubs --stubs ${header})
    else()
        set(stubs "")
        set(header "")
    endif()
    add_custom_command(OUTPUT ${copy}
        COMMAND ${Python3_EXECUTABLE} ${PC_OVERRIDE_SCRIPT} strip --overrides ${PC_OVERRIDES} ${stubs}
            ${pc_override_defines} ${PS2_DIR}/${unit} ${copy}
        DEPENDS ${PS2_DIR}/${unit} ${PC_OVERRIDES} ${PC_OVERRIDE_SOURCES} ${PC_OVERRIDE_SCRIPT} ${header}
        COMMENT "Removing overridden definitions from ps2/src/${unit}"
        VERBATIM)
    list(APPEND PS2_UNITS ${copy})
    list(APPEND PS2_ORIGINALS ${PS2_DIR}/${unit} ${header})
    if(header)
        set_property(SOURCE ${copy} APPEND PROPERTY COMPILE_OPTIONS -include ${header})
        set_property(SOURCE ${copy} APPEND PROPERTY OBJECT_DEPENDS ${header})
    endif()
endforeach()
# A tag that names no definition in ps2/src stops the build: its replacement
# would be unused wherever the two signatures differ.
set(PC_OVERRIDES_CHECKED ${CMAKE_BINARY_DIR}/pc_overrides.checked)
add_custom_command(OUTPUT ${PC_OVERRIDES_CHECKED}
    COMMAND ${Python3_EXECUTABLE} ${PC_OVERRIDE_SCRIPT} check --overrides ${PC_OVERRIDES}
        --stubs ${CMAKE_SOURCE_DIR}/port/include/stubs ${pc_override_defines} ${PS2_DIR}
        -o ${PC_OVERRIDES_CHECKED}
    DEPENDS ${PS2_ORIGINALS} ${PC_OVERRIDES} ${PC_OVERRIDE_SOURCES} ${PC_OVERRIDE_SCRIPT}
    COMMENT "Checking that every PC_OVERRIDE definition replaces one of ps2/src's"
    VERBATIM)

# What both halves are compiled with. The game was written for MWCC, which
# accepts constructs clang rejects by default: pointers truncated to 32-bit
# integers, narrowing in braced initialisers and the register keyword.
add_library(dc_options INTERFACE)
target_compile_definitions(dc_options INTERFACE ${PORT_DEFINITIONS})
target_include_directories(dc_options INTERFACE
    ${CMAKE_SOURCE_DIR}/port/include ${CMAKE_SOURCE_DIR}/ps2/include ${CMAKE_SOURCE_DIR}/ps2/include/sce)
target_compile_options(dc_options INTERFACE
    "SHELL:-include \"${CMAKE_SOURCE_DIR}/port/include/port.h\""
    -fms-extensions -ffunction-sections -fdata-sections
    -Wno-register -Wno-c++11-narrowing -Wno-return-mismatch)
# arm64 clang fuses a*b+c into one rounding by default; x86-64 has no FMA in its
# baseline. Off everywhere so the game's arithmetic rounds the same on both.
target_compile_options(dc_options INTERFACE -ffp-contract=off)

if(WIN32 AND (NOT MINGW OR NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang"))
    message(FATAL_ERROR "The Windows build requires clang/llvm-mingw")
endif()

add_library(dc_ps2 OBJECT ${PS2_UNITS} ${PC_OVERRIDES_CHECKED})
target_link_libraries(dc_ps2 PRIVATE dc_options)
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

# Shared by the game and its tests: the port's objects, the game's and the link
# flags they need. Names the PS2 build tells apart per object
# (ps2/config/*/object_fixups.json) are renamed by port/include/stubs, so no
# name is defined twice. The executable is position-independent on every
# platform: the game's casts of pointers to 32-bit integers are all widened in
# port/src (docs/port/truncations.md), so nothing has to sit below 4 GiB.
# draw_rect and WorkBuffer__2 are second names for existing storage, as the PS2
# build gives them through object_fixups.json. port/src/linknames.cpp defines
# the names that are not aliases, EditGaijiTbl among them. ItemPutListTbl12_bytes
# is not one: port/include/stubs/dngstatusdata.hpp turns GetItem's read through
# it into a WeaponList lookup.
if(WIN32)
    # The two aliases are /alternatename directives in port/src/linknames.cpp.
    set(DC_LINK_OPTIONS -fuse-ld=lld -Wl,--gc-sections -Wl,--error-limit=0)
elseif(APPLE)
    set(DC_LINK_OPTIONS -Wl,-dead_strip -Wl,-alias,_draw_rect_store,_draw_rect
        -Wl,-alias,_WorkBuffer,_WorkBuffer__2)
else()
    set(DC_LINK_OPTIONS -fuse-ld=lld -pie -Wl,-z,separate-code -Wl,--gc-sections -Wl,--error-limit=0
        -Wl,--defsym=draw_rect=draw_rect_store -Wl,--defsym=WorkBuffer__2=WorkBuffer)
endif()

function(dc_link_game target)
    target_sources(${target} PRIVATE $<TARGET_OBJECTS:dc_port> $<TARGET_OBJECTS:dc_host> $<TARGET_OBJECTS:dc_ps2>)
    target_link_libraries(${target} PRIVATE dc_options SDL3::SDL3 Vulkan::Vulkan)
    target_include_directories(${target} PRIVATE ${PORT_DIR})
    target_compile_options(${target} PRIVATE -Wall -Wno-mismatched-tags -Wno-unused-label
        -Wno-overloaded-virtual -Wno-unused-function)
    target_link_options(${target} PRIVATE ${DC_LINK_OPTIONS})
endfunction()

add_executable(darkcloud ${PORT_DIR}/main.cpp)
set_target_properties(darkcloud PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
dc_link_game(darkcloud)
if(WIN32)
    target_link_options(darkcloud PRIVATE -municode)
endif()

include(GNUInstallDirs)
install(TARGETS darkcloud RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# The tests use an installed GoogleTest, or fetch one at configure time; a build with neither turns them off.
option(DC_BUILD_TESTS "Build darkcloud_tests" ON)
if(DC_BUILD_TESTS)
    enable_testing()
    add_subdirectory(src/tests)
endif()
add_subdirectory(${CMAKE_SOURCE_DIR}/tools/dcdata ${CMAKE_BINARY_DIR}/tools/dcdata)
