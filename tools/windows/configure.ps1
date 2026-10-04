param([string]$Root, [string]$BuildDirectory, [switch]$Tests, [string]$BuildType = 'Debug')
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/common.ps1"
$root = (Resolve-WindowsRoot $Root).Replace('\', '/')
$build = (Resolve-WindowsBuild $root $BuildDirectory).Replace('\', '/')
$source = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).Replace('\', '/')
$llvm = "$root/deps/llvm/llvm-mingw-20260922-ucrt-x86_64/bin"
$cmake = Find-WindowsTool cmake 'C:/Program Files/CMake/bin/cmake.exe'
$python = Find-WindowsPython
$testOption = if ($Tests) { 'ON' } else { 'OFF' }
& $cmake -S $source -B $build -G Ninja `
    -DPLATFORM=PC "-DCMAKE_BUILD_TYPE=$BuildType" "-DDC_BUILD_TESTS=$testOption" `
    "-DCMAKE_CXX_COMPILER=$llvm/clang++.exe" `
    "-DCMAKE_MAKE_PROGRAM=$root/deps/ninja/ninja.exe" "-DPython3_EXECUTABLE=$python" `
    "-DSDL3_DIR=$root/deps/sdl/SDL3-3.4.18/x86_64-w64-mingw32/lib/cmake/SDL3" `
    "-DVulkan_INCLUDE_DIR=$root/deps/vulkan/Vulkan-Headers-vulkan-sdk-1.4.363.0/include" `
    "-DVulkan_LIBRARY=$env:SystemRoot/System32/vulkan-1.dll" `
    "-DGLSLANG_VALIDATOR=$root/deps/glslang/bin/glslang.exe"
exit $LASTEXITCODE
