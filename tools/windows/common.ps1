function Resolve-WindowsRoot([string]$Root) {
    if (!$Root) { $Root = Join-Path $PSScriptRoot '../../..' }
    return [IO.Path]::GetFullPath($Root)
}

function Resolve-WindowsBuild([string]$Root, [string]$BuildDirectory) {
    if (!$BuildDirectory) { $BuildDirectory = Join-Path $Root 'build' }
    return [IO.Path]::GetFullPath($BuildDirectory)
}

function Find-WindowsTool([string]$Name, [string]$Fallback) {
    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }
    if (Test-Path -LiteralPath $Fallback) { return $Fallback }
    throw "Install $Name or add it to PATH."
}

function Find-WindowsPython {
    $command = Get-Command py -ErrorAction SilentlyContinue
    if ($command) { $python = & $command.Source -3 -c 'import sys; print(sys.executable)' }
    else { $python = & (Find-WindowsTool python '') -c 'import sys; print(sys.executable)' }
    if ($LASTEXITCODE) { throw 'Cannot locate Python 3' }
    return $python
}
