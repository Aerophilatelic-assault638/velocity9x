[CmdletBinding()]
param(
    [string]$BuildId
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$outputDir = Join-Path $repoRoot "build\host-msvc"

. (Join-Path $PSScriptRoot "common.ps1")
if (-not $BuildId) {
    $BuildId = Get-V9xBuildId -RepoRoot $repoRoot -Fallback "local"
}

$cl = Get-Command "cl.exe" -ErrorAction SilentlyContinue
if (-not $cl) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} `
        "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path -LiteralPath $vswhere) {
        $vsRoot = & $vswhere -latest -products * `
            -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
            -property installationPath
        if ($vsRoot) {
            $devCmd = Join-Path $vsRoot "Common7\Tools\VsDevCmd.bat"
            $envDump = cmd /c "`"$devCmd`" -arch=x64 -no_logo && set"
            foreach ($line in $envDump) {
                if ($line -match '^([^=]+)=(.*)$') {
                    Set-Item -Path "env:$($Matches[1])" -Value $Matches[2]
                }
            }
            $cl = Get-Command "cl.exe" -ErrorAction SilentlyContinue
        }
    }
}
if (-not $cl) {
    throw "MSVC cl.exe was not found. Run from a Developer PowerShell or install the VS Build Tools."
}

New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
$executable = Join-Path $outputDir "v9x-host-tests.exe"
$sourceNames = @(
    "src\common\build.c",
    "src\common\mode.c",
    "src\common\log.c",
    "src\chipsets\s3\virge\backend.c",
    "src\display16\display_component.c",
    "src\minivdd32\minivdd_component.c",
    "tests\host\test_main.c"
)
$sources = @($sourceNames | ForEach-Object { Join-Path $repoRoot $_ })
$arguments = @(
    "/nologo",
    "/W4",
    "/WX",
    "/I$(Join-Path $repoRoot 'include')",
    "/DV9X_BUILD_ID=\`"$BuildId\`"",
    "/Fe$executable"
) + $sources

Push-Location $outputDir
try {
    & $cl.Source @arguments
    if ($LASTEXITCODE -ne 0) {
        throw "MSVC compilation failed with exit code $LASTEXITCODE."
    }
    & $executable
    if ($LASTEXITCODE -ne 0) {
        throw "Host tests failed with exit code $LASTEXITCODE."
    }
}
finally {
    Pop-Location
}
