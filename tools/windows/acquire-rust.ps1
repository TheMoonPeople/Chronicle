#requires -Version 7.0
param([string]$Root, [switch]$SkipRustfmt, [switch]$SkipClippy)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/common.ps1"
$root = Resolve-WindowsRoot $Root
$sevenZip = Find-WindowsTool 7z 'C:/Program Files/7-Zip/7z.exe'
$source = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$version = '1.99.0'
$triple = 'x86_64-pc-windows-gnullvm'
$rustRoot = Join-Path $root 'deps/rust'
$prefix = Join-Path $rustRoot "$version-gnullvm"
$downloads = Join-Path $rustRoot 'downloads'
$stage = Join-Path $rustRoot 'stage'
New-Item -ItemType Directory -Force $downloads,$stage,$prefix | Out-Null

function Get-VerifiedArchive([string]$Name, [string]$Url, [string]$Sha256) {
    $archive = Join-Path $downloads $Name
    if (!(Test-Path -LiteralPath $archive)) {
        & curl.exe -fL --retry 3 -o $archive $Url
        if ($LASTEXITCODE) { throw "Download failed: $Url" }
    }
    $actual = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash
    if ($actual -ne $Sha256) { throw "SHA-256 mismatch: $archive (expected $Sha256, got $actual)" }
    return $archive
}

# Unpacks a .tar.xz into $stage, only Members when given, with 7-Zip as acquire.ps1 unpacks the rest. The tar of
# Windows 10 reads no xz (Windows 11 23H2 added it) and waits forever rather than failing.
function Expand-TarXz([string]$Archive, [string[]]$Members = @()) {
    & $sevenZip x -y "-o$stage" $Archive > $null
    if ($LASTEXITCODE) { throw "Extract failed: $Archive" }
    $tar = Join-Path $stage ([IO.Path]::GetFileNameWithoutExtension($Archive))
    & $sevenZip x -y "-o$stage" $tar @Members > $null
    $status = $LASTEXITCODE
    Remove-Item -LiteralPath $tar -Force
    if ($status) { throw "Extract failed: $tar" }
}

$release = "https://static.rust-lang.org/dist/2026-10-01"
$rustName = "rust-$version-$triple"
$rustArchive = Get-VerifiedArchive "$rustName.tar.xz" "$release/$rustName.tar.xz" `
    '958C1B5B43413C7F7C226234422799A0B13BBC6904659ED3300591762D5A4E55'
if (!(Test-Path -LiteralPath (Join-Path $prefix 'bin/rustc.exe')) -or
    !(Test-Path -LiteralPath (Join-Path $prefix 'bin/cargo.exe')) -or
    !(Test-Path -LiteralPath (Join-Path $prefix "lib/rustlib/$triple/lib"))) {
    Expand-TarXz $rustArchive -Members @("$rustName/rustc", "$rustName/cargo", "$rustName/rust-std-$triple")
    foreach ($component in @('rustc', 'cargo', "rust-std-$triple")) {
        $componentRoot = Join-Path $stage "$rustName/$component"
        foreach ($file in Get-ChildItem -LiteralPath $componentRoot -Recurse -File) {
            $relative = [IO.Path]::GetRelativePath($componentRoot, $file.FullName)
            $destination = Join-Path $prefix $relative
            New-Item -ItemType Directory -Force (Split-Path -Parent $destination) | Out-Null
            Copy-Item -LiteralPath $file.FullName -Destination $destination -Force
        }
    }
}
if ((& (Join-Path $prefix 'bin/rustc.exe') --version) -notmatch '^rustc 1\.99\.0 ') {
    throw 'Workspace Rust compiler version mismatch'
}

if (!$SkipRustfmt) {
    $fmtName = "rustfmt-$version-$triple"
    $fmtArchive = Get-VerifiedArchive "$fmtName.tar.xz" "$release/$fmtName.tar.xz" `
        '574D8389A6F73537357EDCCC2B6137BD934BC4E9351FD8BF5B0F5F5AA48B3C4B'
    if (!(Test-Path -LiteralPath (Join-Path $prefix 'bin/rustfmt.exe'))) {
        Expand-TarXz $fmtArchive
        Copy-Item -Path (Join-Path $stage "$fmtName/rustfmt-preview/bin/*") -Destination (Join-Path $prefix 'bin') -Force
    }
}

if (!$SkipClippy) {
    $clippyName = "clippy-$version-$triple"
    $clippyArchive = Get-VerifiedArchive "$clippyName.tar.xz" "$release/$clippyName.tar.xz" `
        'A6522090DC91603B1E1BAB5E65207DEF32487A49B76300D7DF2A3262E9A6F1BB'
    if (!(Test-Path -LiteralPath (Join-Path $prefix 'bin/cargo-clippy.exe'))) {
        Expand-TarXz $clippyArchive
        Copy-Item -Path (Join-Path $stage "$clippyName/clippy-preview/bin/*") -Destination (Join-Path $prefix 'bin') -Force
    }
}

$corrosionArchive = Get-VerifiedArchive 'corrosion-v0.6.1.zip' `
    'https://github.com/corrosion-rs/corrosion/archive/refs/tags/v0.6.1.zip' `
    '5C1C5C6AB7196FE4466321C14E9698EC2D0A977C89FD9F1639A37FCFFDF604F2'
$corrosion = Join-Path $rustRoot 'corrosion/corrosion-0.6.1'
if (!(Test-Path -LiteralPath (Join-Path $corrosion 'CMakeLists.txt'))) {
    Expand-Archive -LiteralPath $corrosionArchive -DestinationPath (Join-Path $rustRoot 'corrosion') -Force
}

$env:CARGO_HOME = Join-Path $rustRoot 'cargo-home'
$llvmBin = Join-Path $root 'deps/llvm/llvm-mingw-20260922-ucrt-x86_64/bin'
$env:PATH = "$(Join-Path $prefix 'bin');$llvmBin;$env:PATH"
$cargo = Join-Path $prefix 'bin/cargo.exe'
& $cargo fetch --locked --manifest-path (Join-Path $source 'tools/kazarov/Cargo.toml')
if ($LASTEXITCODE) { throw 'Cargo dependency fetch failed' }
$cxxbridgeArchive = Get-VerifiedArchive 'cxxbridge-1.0.205-windows-x64.zip' `
    'https://github.com/Plarpoon/Kazarov/releases/download/deps-1/cxxbridge-1.0.205-windows-x64.zip' `
    '8C9529BFBAAF423DBD3F2F9583C19EA6CA92E1E09D2E7B0C474799F57AA39A21'
if (!(Test-Path -LiteralPath (Join-Path $prefix 'bin/cxxbridge.exe'))) {
    Expand-Archive -LiteralPath $cxxbridgeArchive -DestinationPath (Join-Path $prefix 'bin') -Force
}
Write-Output "Rust $version ($triple), Corrosion 0.6.1, and cxxbridge-cmd 1.0.205 ready in $rustRoot"
