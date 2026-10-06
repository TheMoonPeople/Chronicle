#requires -Version 7.0
param([string]$Root, [switch]$VulkanLoader)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/common.ps1"
$root = Resolve-WindowsRoot $Root
$sevenZip = Find-WindowsTool 7z 'C:/Program Files/7-Zip/7z.exe'
$archives = Join-Path $root 'archives'
$deps = Join-Path $root 'deps'
New-Item -ItemType Directory -Force $archives,$deps | Out-Null
$downloads = @(
    @('llvm', 'https://github.com/mstorsjo/llvm-mingw/releases/download/20260922/llvm-mingw-20260922-ucrt-x86_64.zip', 'E3AD77D117A4BEA19A7A3B333341824D79A5A371004A10E25B8504E7B3047666'),
    @('ninja', 'https://github.com/ninja-build/ninja/releases/download/v1.13.2/ninja-win.zip', '07FC8261B42B20E71D1720B39068C2E14FFCEE6396B76FB7A795FB460B78DC65'),
    @('sdl', 'https://github.com/libsdl-org/SDL/releases/download/release-3.4.18/SDL3-devel-3.4.18-mingw.zip', 'CD98158C8D025A816F430A600B4E9526524E6219EE884EC72BD498B80FD3BF58'),
    @('glslang', 'https://github.com/KhronosGroup/glslang/releases/download/16.6.0/glslang-16.6.0-windows-x86_64-release.zip', '82BF434E69B9BB4829DE7E2B4BC2C5E7A7861E53D66CF75E5CC70F5F694A8D9B'),
    @('vulkan', 'https://github.com/KhronosGroup/Vulkan-Headers/archive/refs/tags/vulkan-sdk-1.4.363.0.zip', 'F4BE95220FF0EE0B1C620301FF4FE9AE48592D71F4C30F2D5C571351A153AE81')
)
if ($VulkanLoader) {
    $downloads += ,@('vulkan-runtime', 'https://sdk.lunarg.com/sdk/download/1.4.363.0/windows/VulkanRT-X64-1.4.363.0-Components.zip', 'A25A927AA8B9F0371048F1861CF88AC3B9BC9B1FB332C42D897C8AB32695769A')
}
$downloads | ForEach-Object -Parallel {
    $name,$url,$expected = $_
    $archive = Join-Path $using:archives "$name.zip"
    $target = Join-Path $using:deps $name
    if (!(Test-Path $archive)) {
        & curl.exe -fL --retry 3 -o $archive $url 2> (Join-Path $using:archives "$name-download.log")
        if ($LASTEXITCODE) { throw "Download failed: $url" }
    }
    $hash = Get-FileHash $archive -Algorithm SHA256
    if ($hash.Hash -ne $expected) { throw "SHA-256 mismatch: $archive (expected $expected, got $($hash.Hash))" }
    & $using:sevenZip x -y "-o$target" $archive > (Join-Path $using:archives "$name-extract.log")
    if ($LASTEXITCODE) { throw "Extract failed: $archive" }
    $hash | Select-Object Path,Hash
} -ThrottleLimit 4
