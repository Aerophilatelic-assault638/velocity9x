[CmdletBinding()]
param(
    [string]$BuildId
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$outputDir = Join-Path $repoRoot "build\dos-diag"

. (Join-Path $PSScriptRoot "common.ps1")
if (-not $BuildId) {
    $BuildId = Get-V9xBuildId -RepoRoot $repoRoot -Fallback "dos-diag-local"
}

$watcomRoot = $env:WATCOM
if (-not $watcomRoot -and (Test-Path -LiteralPath "C:\WATCOM")) {
    $watcomRoot = "C:\WATCOM"
}
if (-not $watcomRoot) {
    throw "Open Watcom was not found. Set WATCOM or install it at C:\WATCOM."
}

$compilerCandidates = @(
    (Join-Path $watcomRoot "binnt64\wcl.exe"),
    (Join-Path $watcomRoot "binnt\wcl.exe")
)
$compiler = $compilerCandidates | Where-Object {
    Test-Path -LiteralPath $_
} | Select-Object -First 1
if (-not $compiler) {
    throw "Open Watcom wcl.exe was not found under $watcomRoot."
}

$env:WATCOM = $watcomRoot
$env:Path = "$(Join-Path $watcomRoot 'binnt64');$(Join-Path $watcomRoot 'binnt');$env:Path"
$env:INCLUDE = Join-Path $watcomRoot "h"

New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
$source = Join-Path $repoRoot "tools\diag\serial_smoke.c"
$executable = Join-Path $outputDir "v9xser.exe"
$arguments = @(
    "-bt=dos", "-ms", "-zq", "-wx",
    "-dV9X_BUILD_ID=`"$BuildId`"",
    "-fe=$executable", $source
)

Push-Location $outputDir
try {
    & $compiler @arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Open Watcom failed to build the DOS serial smoke utility."
    }
}
finally {
    Pop-Location
}

$bytes = [System.IO.File]::ReadAllBytes($executable)
if ($bytes.Length -lt 2 -or $bytes[0] -ne 0x4d -or $bytes[1] -ne 0x5a) {
    throw "The DOS serial smoke output is not an MZ executable."
}
if (-not [System.Text.Encoding]::ASCII.GetString($bytes).Contains($BuildId)) {
    throw "The DOS serial smoke output does not contain the build identifier."
}

Write-Output "Built DOS COM1 smoke utility: $executable"
