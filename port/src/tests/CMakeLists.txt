# darkcloud_tests links the whole port and game so a test can exercise a
# unit against the real game types. Each *_test.cpp holds
# GoogleTest cases; ctest runs one process per case, since the game's state is
# global.
include(FetchContent)
FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.18.0
    GIT_SHALLOW TRUE
    FIND_PACKAGE_ARGS NAMES GTest)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
include(GoogleTest)

file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*_test.cpp)
add_executable(darkcloud_tests ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp ${TEST_SOURCES})
set_target_properties(darkcloud_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
target_include_directories(darkcloud_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/tools/dcdata)
dc_link_game(darkcloud_tests)
kz_port_tests(darkcloud_tests)
target_link_libraries(darkcloud_tests PRIVATE GTest::gtest nlohmann_json::nlohmann_json)

# A test that stalls is cut off; graphics startup can take several seconds on some systems. The clock
# cases measure short sleeps, which the parallel gfx cases skew, so they run on their own.
set(DC_TEST_TIMEOUT 30 CACHE STRING "Seconds a test case may run before it is cut off")
# The cases that need a Vulkan device have the gpu label, so a machine without one runs ctest -LE gpu. They are
# Kazarov's suites named GfxGpu* (its src/cpp/gfx/README.md, "Shared device test classification") and the ones
# that start a device under another name (GfxMisc, GfxTim2 and GfxCachedTarget), and every suite in a Chronicle
# test file that includes gfx_fixture.hpp or tex_fixture.hpp, found here so that a new one is labeled without a
# list to keep. Some start several renderers, each compiling every pipeline (seconds apiece, more on a software
# driver such as lavapipe), so they have their own limit.
set(DC_GPU_TEST_TIMEOUT 120 CACHE STRING "Seconds a test case that needs a Vulkan device may run")
set(device_suites GfxMisc GfxTim2 GfxCachedTarget)
foreach(source IN LISTS TEST_SOURCES)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${source})
    file(STRINGS ${source} fixture REGEX "^#include \"(gfx|tex)_fixture\\.hpp\"")
    if(fixture)
        file(STRINGS ${source} cases REGEX "^TEST(_F|_P)?\\([A-Za-z0-9_]+,")
        list(TRANSFORM cases REPLACE "^TEST(_F|_P)?\\(([A-Za-z0-9_]+),.*" "\\2")
        list(APPEND device_suites ${cases})
    endif()
endforeach()
list(REMOVE_DUPLICATES device_suites)
list(TRANSFORM device_suites APPEND ".*")
list(JOIN device_suites ":" device_filter)
set(device_filter "*GfxGpu*.*:${device_filter}")
gtest_discover_tests(darkcloud_tests
    DISCOVERY_MODE PRE_TEST
    TEST_FILTER -PlatformClock.*:${device_filter}
    PROPERTIES TIMEOUT ${DC_TEST_TIMEOUT} ENVIRONMENT "DC_TEST_FONT=${CMAKE_SOURCE_DIR}/tools/font/ChronicleSharp-Bold.ttf")
gtest_discover_tests(darkcloud_tests
    DISCOVERY_MODE PRE_TEST
    TEST_FILTER ${device_filter}
    PROPERTIES TIMEOUT ${DC_GPU_TEST_TIMEOUT} LABELS gpu
        ENVIRONMENT "DC_TEST_FONT=${CMAKE_SOURCE_DIR}/tools/font/ChronicleSharp-Bold.ttf")
gtest_discover_tests(darkcloud_tests
    DISCOVERY_MODE PRE_TEST
    TEST_FILTER PlatformClock.*
    PROPERTIES TIMEOUT ${DC_TEST_TIMEOUT} ENVIRONMENT "DC_TEST_FONT=${CMAKE_SOURCE_DIR}/tools/font/ChronicleSharp-Bold.ttf" RUN_SERIAL TRUE)
