if ($args.Count -lt 1 -or $args[0] -ne 'switch') {
    Write-Error 'usage: build.bat switch [CMake build arguments...]'
    exit 2
}

$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$wslRoot = (& wsl.exe wslpath -a $root.Replace('\', '/'))
if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($wslRoot)) {
    Write-Error 'Could not resolve the Chronicle checkout in WSL.'
    exit 1
}
$wslRoot = $wslRoot.Trim()

$wslArgs = @('--exec', 'env')
foreach ($name in @('BUILDER', 'CLEAN', 'JOBS', 'SWITCH_PRESET')) {
    $value = [Environment]::GetEnvironmentVariable($name)
    if ($null -ne $value) {
        $wslArgs += "$name=$value"
    }
}
$wslArgs += @('bash', "$wslRoot/build.sh", 'switch')
if ($args.Count -gt 1) {
    $wslArgs += $args[1..($args.Count - 1)]
}

& wsl.exe @wslArgs
exit $LASTEXITCODE
