$ErrorActionPreference = 'Stop'

$androidRoot = $PSScriptRoot
$appLibs = Join-Path $androidRoot 'app/libs'
$hostTools = Join-Path $androidRoot 'host-tools'
New-Item -ItemType Directory -Force -Path $appLibs, $hostTools | Out-Null

$dependencies = @(
    @{
        Uri = 'https://github.com/libsdl-org/SDL/releases/download/release-3.4.16/SDL3-devel-3.4.16-android.zip'
        Archive = Join-Path $env:TEMP 'SDL3-devel-3.4.16-android.zip'
        Sha256 = 'E1DA8D9298471EA4AB8C6D48A804C0AC5415F35F30FCF7EBA61AA7357E165765'
        Target = Join-Path $appLibs 'SDL3-3.4.16.aar'
        InnerPath = 'SDL3-3.4.16.aar'
    },
    @{
        Uri = 'https://github.com/KhronosGroup/glslang/releases/download/16.6.0/glslang-16.6.0-windows-x86_64-release.zip'
        Archive = Join-Path $env:TEMP 'glslang-16.6.0-windows-x86_64-release.zip'
        Sha256 = '82BF434E69B9BB4829DE7E2B4BC2C5E7A7861E53D66CF75E5CC70F5F694A8D9B'
        Target = Join-Path $hostTools 'glslang.exe'
        InnerPath = 'bin/glslang.exe'
    }
)

foreach ($dependency in $dependencies) {
    if (-not (Test-Path -LiteralPath $dependency.Target)) {
        Invoke-WebRequest -Uri $dependency.Uri -OutFile $dependency.Archive
        $actual = (Get-FileHash -LiteralPath $dependency.Archive -Algorithm SHA256).Hash
        if ($actual -ne $dependency.Sha256) {
            throw "SHA-256 mismatch for $($dependency.Uri): $actual"
        }
        $temp = Join-Path $env:TEMP ([guid]::NewGuid().ToString('N'))
        New-Item -ItemType Directory -Path $temp | Out-Null
        try {
            Expand-Archive -LiteralPath $dependency.Archive -DestinationPath $temp
            $inner = Join-Path $temp ($dependency.InnerPath -replace '/', '\')
            Copy-Item -LiteralPath $inner -Destination $dependency.Target
        } finally {
            Remove-Item -LiteralPath $temp -Recurse -Force
        }
    }
}
