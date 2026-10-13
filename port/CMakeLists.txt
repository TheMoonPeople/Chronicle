# The x64 port: the game's code built with clang as C++26 for the host.
#
# ps2/src holds only the game's code as the PS2 build compiles it. A definition
# port/src tags PORT_OVERRIDE replaces the one ps2/src gives the same name: the
# port compiles a copy of each unit without those definitions
# (tools/kazarov/scripts/port_override.py) and links it with port/src. That is how the
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

set(KZ_ROOT ${CMAKE_SOURCE_DIR}/tools/kazarov)
if(NOT EXISTS ${KZ_ROOT}/include/kazarov/port.h)
    message(FATAL_ERROR "${KZ_ROOT} is empty: run git submodule update --init tools/kazarov")
endif()

include(${KZ_ROOT}/cmake/KazarovPort.cmake)

# Python runs ${KZ_ROOT}/scripts/port_override.py.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
# Flatpak builds have no network: their single-file libraries are staged in audio_decoders/ and stb_truetype/; others fetch them.
set(KZ_PORT_VENDOR_DIRS ${CMAKE_SOURCE_DIR}/audio_decoders ${CMAKE_SOURCE_DIR}/stb_truetype)
kz_port_host_library(dc_host)

set(PS2_DIR ${CMAKE_SOURCE_DIR}/ps2/src)
set(PORT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
file(GLOB_RECURSE PS2_SOURCES CONFIGURE_DEPENDS RELATIVE ${PS2_DIR} ${PS2_DIR}/*.cpp)
file(GLOB_RECURSE PORT_SOURCES CONFIGURE_DEPENDS ${PORT_DIR}/*.cpp)
list(FILTER PORT_SOURCES EXCLUDE REGEX "/tests/")
# Temporary files beside a source are not units.
list(FILTER PS2_SOURCES EXCLUDE REGEX "(^|/)tmp[^/]*$")

set(PS2_UNITS "")
# What both halves are compiled with (dc_options below). PORT leaves out the
# functions ps2/src writes in MIPS assembly.
set(PORT_DEFINITIONS PORT PAL)
list(TRANSFORM PORT_DEFINITIONS PREPEND --define= OUTPUT_VARIABLE port_override_defines)
# The names port/src's PORT_OVERRIDE definitions link under. The list is rewritten
# only when it changes, so other edits to port/src rebuild nothing of ps2/src.
# PORT_OVERRIDE_SOURCES changes when a unit or a stub header comes or goes, which no
# timestamp shows.
set(PORT_OVERRIDE_SCRIPT ${KZ_ROOT}/scripts/port_override.py)
set(PORT_OVERRIDES ${CMAKE_BINARY_DIR}/port_overrides.txt)
set(PORT_OVERRIDE_SOURCES ${CMAKE_BINARY_DIR}/port_override_sources.txt)
file(GLOB_RECURSE PORT_OVERRIDE_STUBS CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/port/include/stubs/*.hpp)
string(JOIN "\n" port_override_sources ${PORT_SOURCES} ${PS2_SOURCES} ${PORT_OVERRIDE_STUBS})
file(CONFIGURE OUTPUT ${PORT_OVERRIDE_SOURCES} CONTENT "${port_override_sources}\n" @ONLY)
add_custom_command(OUTPUT ${PORT_OVERRIDES}
    COMMAND ${Python3_EXECUTABLE} ${PORT_OVERRIDE_SCRIPT} list ${PORT_DIR} -o ${PORT_OVERRIDES}
    DEPENDS ${PORT_SOURCES} ${PORT_OVERRIDE_SOURCES} ${PORT_OVERRIDE_SCRIPT}
    COMMENT "Listing port/src's PORT_OVERRIDE definitions"
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
        COMMAND ${Python3_EXECUTABLE} ${PORT_OVERRIDE_SCRIPT} strip --overrides ${PORT_OVERRIDES} ${stubs}
            ${port_override_defines} ${PS2_DIR}/${unit} ${copy}
        DEPENDS ${PS2_DIR}/${unit} ${PORT_OVERRIDES} ${PORT_OVERRIDE_SOURCES} ${PORT_OVERRIDE_SCRIPT} ${header}
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
set(PORT_OVERRIDES_CHECKED ${CMAKE_BINARY_DIR}/port_overrides.checked)
add_custom_command(OUTPUT ${PORT_OVERRIDES_CHECKED}
    COMMAND ${Python3_EXECUTABLE} ${PORT_OVERRIDE_SCRIPT} check --overrides ${PORT_OVERRIDES}
        --stubs ${CMAKE_SOURCE_DIR}/port/include/stubs ${port_override_defines} ${PS2_DIR}
        -o ${PORT_OVERRIDES_CHECKED}
    DEPENDS ${PS2_ORIGINALS} ${PORT_OVERRIDES} ${PORT_OVERRIDE_SOURCES} ${PORT_OVERRIDE_SCRIPT}
    COMMENT "Checking that every PORT_OVERRIDE definition replaces one of ps2/src's"
    VERBATIM)

# What both halves are compiled with. The game was written for MWCC, which
# accepts constructs clang rejects by default: pointers truncated to 32-bit
# integers, narrowing in braced initialisers and the register keyword.
add_library(dc_options INTERFACE)
target_compile_definitions(dc_options INTERFACE ${PORT_DEFINITIONS})
target_include_directories(dc_options INTERFACE
    ${CMAKE_SOURCE_DIR}/port/include ${KZ_ROOT}/include ${CMAKE_SOURCE_DIR}/ps2/include ${CMAKE_SOURCE_DIR}/ps2/include/sce)
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

add_library(dc_ps2 OBJECT ${PS2_UNITS} ${PORT_OVERRIDES_CHECKED})
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

set(SHADER_HEADER_DIR ${CMAKE_BINARY_DIR}/generated)

# port/icon/chronicle-128.rgba, the window icon's raw 128x128 RGBA pixels, as a byte array.
set(ICON_RGBA ${CMAKE_SOURCE_DIR}/port/icon/chronicle-128.rgba)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${ICON_RGBA})
file(READ ${ICON_RGBA} icon_hex HEX)
string(REGEX REPLACE "(..)" "0x\\1," icon_bytes "${icon_hex}")
file(CONFIGURE OUTPUT ${SHADER_HEADER_DIR}/icon/chronicle.hpp
    CONTENT "#pragma once\n#include <cstdint>\ninline constexpr int kIconSize = 128;\ninline constexpr std::uint8_t kIconRgba[] = {${icon_bytes}};\n")

# CREDITS, one contributor a line and # for comments, as the About page's list of names.
set(CREDITS_FILE ${CMAKE_SOURCE_DIR}/CREDITS)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CREDITS_FILE})
file(STRINGS ${CREDITS_FILE} credit_lines ENCODING UTF-8)
set(credit_names "")
foreach(line IN LISTS credit_lines)
    string(STRIP "${line}" line)
    if(line STREQUAL "" OR line MATCHES "^#")
        continue()
    endif()
    string(REPLACE "\\" "\\\\" line "${line}")
    string(REPLACE "\"" "\\\"" line "${line}")
    string(APPEND credit_names "\"${line}\", ")
endforeach()
if(credit_names STREQUAL "")
    set(credit_names "\"\"")
endif()
file(CONFIGURE OUTPUT ${SHADER_HEADER_DIR}/options/credits.hpp
    CONTENT "#pragma once\ninline constexpr const char *kCredits[] = {${credit_names}};\n")

kz_port_sce_sources(DC_SCE_SOURCES LIBS libvu0 libpad libmc sifdev sifrpc eekernel libcdvd libgraph libdma libpkt)
add_library(dc_port OBJECT ${PORT_LIB_SOURCES} ${DC_SCE_SOURCES})
target_link_libraries(dc_port PRIVATE dc_options SDL3::SDL3 Vulkan::Vulkan nlohmann_json::nlohmann_json kz_port_headers)
target_include_directories(dc_port PRIVATE ${PORT_DIR} ${SHADER_HEADER_DIR})
# captionfont.cpp rasterizes with stb_truetype, which Kazarov stages (or fetches) for dc_host.
get_target_property(dc_host_includes dc_host INCLUDE_DIRECTORIES)
foreach(include_dir IN LISTS dc_host_includes)
    if(EXISTS ${include_dir}/stb_truetype.h)
        target_include_directories(dc_port PRIVATE ${include_dir})
    endif()
endforeach()
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
    target_link_libraries(${target} PRIVATE dc_options kz_port_headers SDL3::SDL3 Vulkan::Vulkan)
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
    enable_language(RC)
    target_sources(darkcloud PRIVATE ${CMAKE_SOURCE_DIR}/port/icon/chronicle.rc)
endif()

include(GNUInstallDirs)
install(TARGETS darkcloud RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# The strings the port draws itself, translated (docs/LOCALIZATION.md): lang/ beside the executable, where the
# game reads its language files.
add_custom_target(dc_lang
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/lang $<TARGET_FILE_DIR:darkcloud>/lang
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/tools/font/DarkCloudCompendium.ttf
            $<TARGET_FILE_DIR:darkcloud>/lang/font.ttf
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/tools/font/DarkCloudCompendiumThin.ttf
            $<TARGET_FILE_DIR:darkcloud>/lang/font_thin.ttf
    COMMENT "Copying the port's translated strings and message font")
add_dependencies(darkcloud dc_lang)
install(DIRECTORY lang/ DESTINATION ${CMAKE_INSTALL_BINDIR}/lang)
install(FILES ${CMAKE_SOURCE_DIR}/tools/font/DarkCloudCompendium.ttf DESTINATION ${CMAKE_INSTALL_BINDIR}/lang RENAME font.ttf)
install(FILES ${CMAKE_SOURCE_DIR}/tools/font/DarkCloudCompendiumThin.ttf DESTINATION ${CMAKE_INSTALL_BINDIR}/lang RENAME font_thin.ttf)

# The button symbols (docs/GLYPHS.md): glyphs/ beside the executable. Rebuilt from art by tools/glyphs/build_glyphs.py.
add_custom_target(dc_glyphs
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/glyphs $<TARGET_FILE_DIR:darkcloud>/glyphs
    COMMENT "Copying the button symbols")
add_dependencies(darkcloud dc_glyphs)
install(DIRECTORY glyphs/ DESTINATION ${CMAKE_INSTALL_BINDIR}/glyphs)

# The tests use an installed GoogleTest, or fetch one at configure time; a build with neither turns them off.
option(DC_BUILD_TESTS "Build darkcloud_tests" ON)
if(DC_BUILD_TESTS)
    enable_testing()
    add_subdirectory(src/tests)
endif()
add_subdirectory(${CMAKE_SOURCE_DIR}/tools/dcdata ${CMAKE_BINARY_DIR}/tools/dcdata)
