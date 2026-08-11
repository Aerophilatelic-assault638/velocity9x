[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$outputDir = Join-Path $repoRoot "build\matrox-guard-autoexec"
$watcomRoot = if ($env:WATCOM) { $env:WATCOM } else { "C:\WATCOM" }
$compiler = Join-Path $watcomRoot "binnt64\wcc386.exe"
$linker = Join-Path $watcomRoot "binnt64\wlink.exe"
$dumper = Join-Path $watcomRoot "binnt64\wdump.exe"
$kernel32 = Join-Path $watcomRoot "lib386\nt\kernel32.lib"
foreach ($required in @($compiler, $linker, $dumper, $kernel32)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "Missing Matrox AUTOEXEC helper input: $required"
    }
}

$env:WATCOM = $watcomRoot
$env:Path = "$(Join-Path $watcomRoot 'binnt64');$(Join-Path $watcomRoot 'binnt');$env:Path"
$env:INCLUDE = "$(Join-Path $watcomRoot 'h');$(Join-Path $watcomRoot 'h\nt')"
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
$source = Join-Path $repoRoot "tools\diag\matrox_guard_autoexec_win32.c"
$object = Join-Path $outputDir "matrox_guard_autoexec_win32.obj"
$executable = Join-Path $outputDir "v9xauto.exe"
$linkFile = Join-Path $outputDir "v9xauto.lnk"

& $compiler "-bt=nt" "-zq" "-wx" "-zl" "-s" "-fo=$object" $source
if ($LASTEXITCODE -ne 0) {
    throw "Open Watcom failed to compile the Matrox AUTOEXEC helper."
}
$linkLines = @(
    "format windows nt", "runtime windows=4.0", "option quiet",
    "option nodefaultlibs",
    "option start='_V9xMatroxGuardAutoexecEntry@0'",
    "option stack=16384", "name '$executable'", "file '$object'",
    "library '$kernel32'")
Set-Content -LiteralPath $linkFile -Value $linkLines -Encoding Ascii
& $linker "@$linkFile"
if ($LASTEXITCODE -ne 0) {
    throw "Open Watcom failed to link the Matrox AUTOEXEC helper."
}

$dump = (@(& $dumper -e $executable 2>&1)) -join "`n"
foreach ($api in @("CreateFileA", "ReadFile", "WriteFile",
                    "FlushFileBuffers", "ExitProcess")) {
    if ($dump -notmatch "(?m)\s$([regex]::Escape($api))\s*$") {
        throw "Matrox AUTOEXEC helper is missing import $api."
    }
}
if ($dump -match "GetCommandLineW|GetModuleFileNameW|__CHK") {
    throw "Matrox AUTOEXEC helper contains an incompatible runtime import."
}
Write-Output "Built Win98 Matrox AUTOEXEC helper: $executable"
