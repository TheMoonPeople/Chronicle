# The test runner (runner.c): a netloader that stays, built with devkitPro's Switch toolchain.
#   cmake -S tools/switch/runner -B build/runner -G Ninja -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/Switch.cmake
cmake_minimum_required(VERSION 3.25)
project(chronicle_runner C)

find_package(ZLIB REQUIRED)
add_executable(runner runner.c)
target_link_libraries(runner PRIVATE ZLIB::ZLIB deko3d)
nx_generate_nacp(${CMAKE_CURRENT_BINARY_DIR}/runner.nacp NAME "Chronicle test runner" AUTHOR "Chronicle contributors"
    VERSION "0.1.0")
nx_create_nro(runner NACP ${CMAKE_CURRENT_BINARY_DIR}/runner.nacp OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/runner.nro)
