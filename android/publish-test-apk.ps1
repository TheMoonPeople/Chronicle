[CmdletBinding()]
param(
    [Parameter()]
    [string]$DestinationRoot = 'Z:\'
)

$ErrorActionPreference = 'Stop'
$androidRoot = $PSScriptRoot
$apkPath = Join-Path $androidRoot 'app/build/outputs/apk/debug/app-debug.apk'
$gradlePath = Join-Path $androidRoot 'gradlew.bat'
$buildFile = Join-Path $androidRoot 'app/build.gradle'

if (-not (Test-Path -LiteralPath $DestinationRoot -PathType Container)) {
    throw "Test APK destination root is unavailable: $DestinationRoot"
}

$versionLine = Select-String -LiteralPath $buildFile -Pattern "^\s*versionName\s+'([^']+)'" | Select-Object -First 1
if ($null -eq $versionLine -or $versionLine.Line -notmatch "versionName\s+'([^']+)'") {
    throw "Could not read Android versionName from $buildFile"
}
$version = $Matches[1] -replace '[^A-Za-z0-9.-]', '_'

Push-Location -LiteralPath $androidRoot
try {
    & $gradlePath --no-daemon assembleDebug
    if ($LASTEXITCODE -ne 0) {
        throw "assembleDebug failed with exit code $LASTEXITCODE"
    }
} finally {
    Pop-Location
}

if (-not (Test-Path -LiteralPath $apkPath -PathType Leaf)) {
    throw "Gradle completed but no APK was produced at $apkPath"
}

$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$fileStem = "Chronicle_DarkCloud_Android_ARM64-x86_64_v$($version)_debug-test_$stamp"
$archiveRoot = Join-Path $DestinationRoot 'DARKCLOUDBUILDS'
$apkDestination = Join-Path $archiveRoot "$fileStem.apk"
$zipDestination = Join-Path $DestinationRoot "$fileStem.zip"
if ((Test-Path -LiteralPath $apkDestination) -or (Test-Path -LiteralPath $zipDestination)) {
    throw "Refusing to overwrite an existing test build named $fileStem"
}

New-Item -ItemType Directory -Path $archiveRoot -Force | Out-Null
$stagingZip = Join-Path $DestinationRoot ".$fileStem.zip.tmp"
try {
    Compress-Archive -LiteralPath $apkPath -DestinationPath $stagingZip -CompressionLevel Optimal

    # Keep only the newest downloadable ZIP in the share root. Preserve all previous
    # Chronicle/Dark Cloud packages, plus this build's raw APK, in the archive folder.
    $oldBuilds = Get-ChildItem -LiteralPath $DestinationRoot -File |
        Where-Object {
            $_.Extension -in '.apk', '.zip' -and
            ($_.Name -like 'Chronicle_DarkCloud_Android_*' -or $_.Name -like 'DarkCloud-Android-*')
        }
    foreach ($oldBuild in $oldBuilds) {
        Move-Item -LiteralPath $oldBuild.FullName -Destination $archiveRoot
    }

    Move-Item -LiteralPath $stagingZip -Destination $zipDestination
    Move-Item -LiteralPath $apkPath -Destination $apkDestination
} finally {
    if (Test-Path -LiteralPath $stagingZip) {
        Remove-Item -LiteralPath $stagingZip
    }
}

$item = Get-Item -LiteralPath $zipDestination
$hash = (Get-FileHash -LiteralPath $zipDestination -Algorithm SHA256).Hash
[pscustomobject]@{
    ZipPath = $item.FullName
    ArchivedApkPath = $apkDestination
    SizeBytes = $item.Length
    SHA256 = $hash
}
