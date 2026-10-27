# dcdata extracts the game's data from a disc image into a plain directory.
# It shares the port's language settings but none of the game's headers.
cmake_minimum_required(VERSION 3.20)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(dcdata LANGUAGES CXX)
endif()

if(NOT DEFINED KAZAROV_SOURCE_DIR)
    set(KAZAROV_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../kazarov")
endif()
include(${KAZAROV_SOURCE_DIR}/cmake/KazarovRust.cmake)
if(NOT TARGET nlohmann_json::nlohmann_json)
    find_package(nlohmann_json 3 CONFIG REQUIRED)
endif()

file(GLOB DCDATA_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
add_executable(dcdata ${DCDATA_SOURCES})
set_target_properties(dcdata PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
if(MSVC)
    target_compile_options(dcdata PRIVATE /W4)
else()
    target_compile_options(dcdata PRIVATE -Wall -Wextra)
    target_link_options(dcdata PRIVATE -fuse-ld=lld)
endif()
target_link_libraries(dcdata PRIVATE kazarov_bridge kazarov_core nlohmann_json::nlohmann_json)

include(GNUInstallDirs)
install(TARGETS dcdata RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
