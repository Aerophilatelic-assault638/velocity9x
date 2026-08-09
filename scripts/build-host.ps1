[CmdletBinding()]
param(
    [string]$BuildId
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$outputDir = Join-Path $repoRoot "build\host"

. (Join-Path $PSScriptRoot "common.ps1")
if (-not $BuildId) {
    $BuildId = Get-V9xBuildId -RepoRoot $repoRoot -Fallback "local"
}

$watcomRoot = $env:WATCOM
if (-not $watcomRoot -and (Test-Path -LiteralPath "C:\WATCOM")) {
    $watcomRoot = "C:\WATCOM"
}
if ($watcomRoot) {
    $env:WATCOM = $watcomRoot
    $env:Path = "$(Join-Path $watcomRoot 'binnt64');$(Join-Path $watcomRoot 'binnt');$env:Path"
    $env:INCLUDE = "$(Join-Path $watcomRoot 'h');$(Join-Path $watcomRoot 'h\nt')"
}

$compiler = Get-Command "wcl386.exe" -ErrorAction SilentlyContinue
if (-not $compiler -and $watcomRoot) {
    $candidates = @(
        (Join-Path $watcomRoot "binnt64\wcl386.exe"),
        (Join-Path $watcomRoot "binnt\wcl386.exe")
    )
    $compiler = $candidates | Where-Object { Test-Path -LiteralPath $_ } |
        Select-Object -First 1
}
if (-not $compiler) {
    throw "Open Watcom wcl386.exe was not found. Set WATCOM or install it at C:\WATCOM."
}

New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
$executable = Join-Path $outputDir "v9x-host-tests.exe"
$sourceNames = @(
    "src\common\build.c",
    "src\common\mode.c",
    "src\common\log.c",
    "src\common\resources.c",
    "src\chipsets\s3\virge\backend.c",
    "src\chipsets\s3\virge\clocks.c",
    "src\display16\display_component.c",
    "src\minivdd32\minivdd_component.c",
    "tests\host\test_main.c"
)
$sources = @($sourceNames | ForEach-Object { Join-Path $repoRoot $_ })
$arguments = @(
    "-bt=nt",
    "-zq",
    "-wx",
    "-i=$(Join-Path $repoRoot 'include')",
    "-dV9X_BUILD_ID=`"$BuildId`"",
    "-fe=$executable"
) + $sources

Push-Location $outputDir
try {
    & $compiler @arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Open Watcom compilation failed with exit code $LASTEXITCODE."
    }
    & $executable
    if ($LASTEXITCODE -ne 0) {
        throw "Host tests failed with exit code $LASTEXITCODE."
    }
}
finally {
    Pop-Location
}
