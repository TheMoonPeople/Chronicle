# The x64 port: the game's code built with clang as C++26 for the host.
#
# port/src/game and port/include/game are the port's own copy of the game's
# code, forked from ps2/src and ps2/include; the rest of port/src is the
# platform layer it runs on. ps2/ is the matching decompilation alone and the
# port does not compile it.

if(NOT CMAKE_CXX_COMPILER)
    set(CMAKE_CXX_COMPILER clang++)
endif()
enable_language(CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NINTENDO_SWITCH)
    # newlib exposes some POSIX declarations only outside strict ISO mode.
    set(CMAKE_CXX_EXTENSIONS ON)
else()
    set(CMAKE_CXX_EXTENSIONS OFF)
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# darkcloud is position-independent (DC_LINK_OPTIONS below).
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(KZ_ROOT "${KAZAROV_SOURCE_DIR}")
if(NOT EXISTS ${KZ_ROOT}/include/kazarov/port.h)
    message(FATAL_ERROR "${KZ_ROOT} is empty: run git submodule update --init tools/kazarov")
endif()

# Archives Kazarov's FetchContent_Declare downloads take their extraction time, so a changed URL
# rebuilds what they hold. Set before the include: its functions keep the policies they were defined under.
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()
include(${KZ_ROOT}/cmake/KazarovPort.cmake)

if(NINTENDO_SWITCH)
    include(${CMAKE_CURRENT_SOURCE_DIR}/switch/switch.cmake)
endif()

# Python runs the port override script from the Kazarov submodule.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
# Flatpak builds have no network: their single-file libraries are staged in audio_decoders/ and stb_truetype/; others fetch them.
set(KZ_PORT_VENDOR_DIRS ${CMAKE_SOURCE_DIR}/audio_decoders ${CMAKE_SOURCE_DIR}/stb_truetype)
# The Flatpak also stages each library Kazarov's FetchContent_Declare downloads, in flatpak-deps/<name>
# (scripts/port/flatpak_sources.py checks the manifest), and FetchContent takes it from there. Other
# builds have no flatpak-deps/ and download them as declared.
file(GLOB staged_dependencies LIST_DIRECTORIES true "${CMAKE_SOURCE_DIR}/flatpak-deps/*")
foreach(staged IN LISTS staged_dependencies)
    if(IS_DIRECTORY "${staged}")
        get_filename_component(dependency "${staged}" NAME)
        string(TOUPPER "${dependency}" dependency_upper)
        set(FETCHCONTENT_SOURCE_DIR_${dependency_upper} "${staged}")
    endif()
endforeach()
kz_port_host_library(dc_host)
if(NINTENDO_SWITCH)
    dc_switch_host(dc_host)
endif()
# Chronicle still loads retail HD/BD/SQ audio. These compatibility declarations extend the shared
# semantic audio API; every host and game unit must see the same class definitions.
target_include_directories(dc_host BEFORE PRIVATE ${CMAKE_SOURCE_DIR}/port/include)

set(GAME_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/game)
set(PORT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS ${GAME_DIR}/*.cpp)
file(GLOB_RECURSE PORT_SOURCES CONFIGURE_DEPENDS ${PORT_DIR}/*.cpp)
list(FILTER PORT_SOURCES EXCLUDE REGEX "/tests/|^${GAME_DIR}/")
if(NINTENDO_SWITCH)
    list(FILTER PORT_SOURCES EXCLUDE REGEX "/platform/(discord|firstrun)\.cpp$")
endif()
set(PORT_DEFINITIONS PORT PAL)

# What both halves are compiled with. The game was written for MWCC, which
# accepts constructs clang rejects by default: pointers truncated to 32-bit
# integers, narrowing in braced initialisers and the register keyword.
add_library(dc_options INTERFACE)
target_compile_definitions(dc_options INTERFACE ${PORT_DEFINITIONS})
target_include_directories(dc_options INTERFACE
    ${CMAKE_SOURCE_DIR}/port/include ${KZ_ROOT}/include ${CMAKE_SOURCE_DIR}/port/include/game ${CMAKE_SOURCE_DIR}/port/include/game/sce)
target_compile_options(dc_options INTERFACE
    "SHELL:-include \"${CMAKE_SOURCE_DIR}/port/include/port.h\""
    -fms-extensions -ffunction-sections -fdata-sections -Wno-register)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(dc_options INTERFACE -fpermissive -Wno-narrowing -fno-aggressive-loop-optimizations)
else()
    target_compile_options(dc_options INTERFACE -Wno-c++11-narrowing -Wno-return-mismatch)
endif()
# arm64 clang fuses a*b+c into one rounding by default; x86-64 has no FMA in its
# baseline. Off everywhere so the game's arithmetic rounds the same on both.
target_compile_options(dc_options INTERFACE -ffp-contract=off)
if(ANDROID)
    # PS2's s8 is plain char; arm64 clang otherwise treats its table sentinels as unsigned.
    target_compile_options(dc_options INTERFACE -fsigned-char)
endif()

if(WIN32 AND (NOT MINGW OR NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang"))
    message(FATAL_ERROR "The Windows build requires clang/llvm-mingw")
endif()

# The game units may include the platform layer's headers (port/src and the generated headers), so
# dc_ps2 gets dc_port's include path and libraries; dc_port_includes below adds the include path.
add_library(dc_ps2 OBJECT ${GAME_SOURCES})
target_link_libraries(dc_ps2 PRIVATE dc_options SDL3::SDL3 Vulkan::Vulkan nlohmann_json::nlohmann_json kz_port_headers)
# Retail's code reads locals it never set and falls off the end of functions
# that return a value, and ps2/src keeps both because the match depends on
# them. MWCC gave each a defined if arbitrary result; these flags give the port
# a defined one too, along with the type punning and wraparound the code assumes.
target_compile_options(dc_ps2 PRIVATE -ftrivial-auto-var-init=zero -fno-strict-aliasing -fwrapv)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(dc_ps2 PRIVATE -Werror=return-type)
else()
    target_compile_options(dc_ps2 PRIVATE -fno-strict-return)
endif()
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
if(ANDROID)
    target_compile_options(dc_ps2 PRIVATE -Wno-format-security -Wno-return-type)
endif()

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
if(NINTENDO_SWITCH)
    dc_switch_port(dc_port)
endif()
target_link_libraries(dc_port PRIVATE dc_options SDL3::SDL3 Vulkan::Vulkan nlohmann_json::nlohmann_json kz_port_headers)
# port/src, the generated headers and the stb directories: captionfont.cpp rasterizes with
# stb_truetype and platform/png_image.cpp decodes with stb_image, which Kazarov stages (or fetches)
# for dc_host.
function(dc_port_includes target)
    target_include_directories(${target} PRIVATE ${PORT_DIR} ${SHADER_HEADER_DIR})
    get_target_property(dc_host_includes dc_host INCLUDE_DIRECTORIES)
    foreach(include_dir IN LISTS dc_host_includes)
        if(EXISTS ${include_dir}/stb_truetype.h OR EXISTS ${include_dir}/stb_image.h)
            target_include_directories(${target} PRIVATE ${include_dir})
        endif()
    endforeach()
endfunction()
dc_port_includes(dc_port)
dc_port_includes(dc_ps2)
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
# port/src/linknames.cpp defines names the PS2 build gives through
# object_fixups.json, EditGaijiTbl among them. ItemPutListTbl12_bytes is not
# one: port/include/stubs/dngstatusdata.hpp turns GetItem's read through it
# into a WeaponList lookup.
if(NINTENDO_SWITCH)
    set(DC_LINK_OPTIONS -Wl,--gc-sections)
elseif(WIN32)
    set(DC_LINK_OPTIONS -fuse-ld=lld -Wl,--gc-sections -Wl,--error-limit=0)
elseif(APPLE)
    set(DC_LINK_OPTIONS -Wl,-dead_strip)
else()
    set(DC_LINK_OPTIONS -fuse-ld=lld -Wl,--gc-sections -Wl,--error-limit=0)
    if(ANDROID)
        list(APPEND DC_LINK_OPTIONS -Wl,-z,max-page-size=16384)
    else()
        list(APPEND DC_LINK_OPTIONS -pie -Wl,-z,separate-code)
    endif()
endif()

function(dc_link_game target)
    target_sources(${target} PRIVATE $<TARGET_OBJECTS:dc_port> $<TARGET_OBJECTS:dc_ps2>)
    target_link_libraries(${target} PRIVATE dc_options dc_host kz_port_headers SDL3::SDL3 Vulkan::Vulkan fastgltf::fastgltf)
    target_include_directories(${target} PRIVATE ${PORT_DIR})
    target_compile_options(${target} PRIVATE -Wall -Wno-mismatched-tags -Wno-unused-label
        -Wno-overloaded-virtual -Wno-unused-function)
    target_link_options(${target} PRIVATE ${DC_LINK_OPTIONS})
endfunction()

if(ANDROID)
    add_library(main SHARED ${PORT_DIR}/main.cpp)
    target_link_libraries(main PRIVATE log)
    set_target_properties(main PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
    set(darkcloud_target main)
else()
    add_executable(darkcloud ${PORT_DIR}/main.cpp)
    set_target_properties(darkcloud PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
    set(darkcloud_target darkcloud)
endif()
dc_link_game(${darkcloud_target})
if(NINTENDO_SWITCH)
    dc_switch_nro(darkcloud)
endif()
if(WIN32)
    target_link_options(darkcloud PRIVATE -municode)
    enable_language(RC)
    target_sources(darkcloud PRIVATE ${CMAKE_SOURCE_DIR}/port/icon/chronicle.rc)
endif()

include(GNUInstallDirs)
if(NOT ANDROID AND NOT NINTENDO_SWITCH)
    install(TARGETS darkcloud RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

# The strings the port draws itself, translated (docs/LOCALIZATION.md): lang/ beside the executable, where the
# game reads its language files.
if(ANDROID)
    set(ANDROID_ASSETS ${CMAKE_SOURCE_DIR}/port/android/app/src/main/assets)
    file(MAKE_DIRECTORY ${ANDROID_ASSETS}/lang ${ANDROID_ASSETS}/glyphs)
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/lang/ DESTINATION ${ANDROID_ASSETS}/lang)
    configure_file(${CMAKE_SOURCE_DIR}/tools/font/ChronicleSharp-Bold.ttf ${ANDROID_ASSETS}/lang/font.ttf COPYONLY)
    configure_file(${CMAKE_SOURCE_DIR}/tools/font/ChronicleSharp-Regular.ttf ${ANDROID_ASSETS}/lang/font_thin.ttf COPYONLY)
else()
    add_custom_target(dc_lang
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/lang $<TARGET_FILE_DIR:darkcloud>/lang
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/tools/font/ChronicleSharp-Bold.ttf
                $<TARGET_FILE_DIR:darkcloud>/lang/font.ttf
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/tools/font/ChronicleSharp-Regular.ttf
                $<TARGET_FILE_DIR:darkcloud>/lang/font_thin.ttf
        COMMENT "Copying the port's translated strings and text fonts")
    add_dependencies(${darkcloud_target} dc_lang)
    install(DIRECTORY lang/ DESTINATION ${CMAKE_INSTALL_BINDIR}/lang)
    install(FILES ${CMAKE_SOURCE_DIR}/tools/font/ChronicleSharp-Bold.ttf DESTINATION ${CMAKE_INSTALL_BINDIR}/lang RENAME font.ttf)
    install(FILES ${CMAKE_SOURCE_DIR}/tools/font/ChronicleSharp-Regular.ttf DESTINATION ${CMAKE_INSTALL_BINDIR}/lang RENAME font_thin.ttf)
endif()

# The button symbols (docs/GLYPHS.md): glyphs/ beside the executable. Rebuilt from art by tools/glyphs/build_glyphs.py.
if(ANDROID)
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/glyphs/ DESTINATION ${ANDROID_ASSETS}/glyphs)
else()
    add_custom_target(dc_glyphs
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/glyphs $<TARGET_FILE_DIR:darkcloud>/glyphs
        COMMENT "Copying the button symbols")
    add_dependencies(${darkcloud_target} dc_glyphs)
    install(DIRECTORY glyphs/ DESTINATION ${CMAKE_INSTALL_BINDIR}/glyphs)
endif()

# The tests use an installed GoogleTest, or fetch one at configure time; a build with neither turns them off.
option(DC_BUILD_TESTS "Build darkcloud_tests" ON)
if(DC_BUILD_TESTS AND NOT ANDROID AND NOT NINTENDO_SWITCH)
    enable_testing()
    add_subdirectory(src/tests)
endif()
if(NOT ANDROID AND NOT NINTENDO_SWITCH)
    add_subdirectory(${CMAKE_SOURCE_DIR}/tools/dcdata ${CMAKE_BINARY_DIR}/tools/dcdata)
endif()
