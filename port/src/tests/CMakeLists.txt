# darkcloud_tests links the whole port and game so a test can exercise a
# replacement unit against the real game types. Each *_test.cpp holds
# GoogleTest cases; ctest runs one process per case, since the game's state is
# global.
include(FetchContent)
FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.18.0
    GIT_SHALLOW TRUE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
include(GoogleTest)

file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*_test.cpp)
if(APPLE)
    list(FILTER TEST_SOURCES EXCLUDE REGEX "/title_layout_test\\.cpp$")
endif()
add_executable(darkcloud_tests ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp ${TEST_SOURCES})
set_target_properties(darkcloud_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
target_include_directories(darkcloud_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
dc_link_game(darkcloud_tests)
target_link_libraries(darkcloud_tests PRIVATE GTest::gtest)

# A case is a unit test: it finishes in milliseconds, and one that does not is cut off. The clock
# cases measure short sleeps, which the parallel gfx cases skew, so they run on their own.
set(DC_TEST_TIMEOUT 5 CACHE STRING "Seconds a test case may run before it is cut off")
gtest_discover_tests(darkcloud_tests
    DISCOVERY_MODE PRE_TEST
    TEST_FILTER -PlatformClock.*
    PROPERTIES TIMEOUT ${DC_TEST_TIMEOUT})
gtest_discover_tests(darkcloud_tests
    DISCOVERY_MODE PRE_TEST
    TEST_FILTER PlatformClock.*
    PROPERTIES TIMEOUT ${DC_TEST_TIMEOUT} RUN_SERIAL TRUE)
