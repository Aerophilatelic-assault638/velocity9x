[CmdletBinding()]
param(
    [string]$BuildId
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$outputDir = Join-Path $repoRoot "build\win32-diag"

. (Join-Path $PSScriptRoot "common.ps1")
if (-not $BuildId) {
    $BuildId = Get-V9xBuildId -RepoRoot $repoRoot -Fallback "win32-diag-local"
}

$watcomRoot = $env:WATCOM
if (-not $watcomRoot -and (Test-Path -LiteralPath "C:\WATCOM")) {
    $watcomRoot = "C:\WATCOM"
}
if (-not $watcomRoot) {
    throw "Open Watcom was not found. Set WATCOM or install it at C:\WATCOM."
}

$toolDirectories = @(
    (Join-Path $watcomRoot "binnt64"),
    (Join-Path $watcomRoot "binnt")
)
$toolDirectory = $toolDirectories | Where-Object {
    Test-Path -LiteralPath (Join-Path $_ "wcc386.exe")
} | Select-Object -First 1
if (-not $toolDirectory) {
    throw "Open Watcom wcc386.exe was not found under $watcomRoot."
}
$compiler = Join-Path $toolDirectory "wcc386.exe"
$linker = Join-Path $toolDirectory "wlink.exe"
$dumper = Join-Path $toolDirectory "wdump.exe"
foreach ($tool in @($linker, $dumper)) {
    if (-not (Test-Path -LiteralPath $tool)) {
        throw "Required Open Watcom tool was not found: $tool"
    }
}

$kernelLibrary = Join-Path $watcomRoot "lib386\nt\kernel32.lib"
$userLibrary = Join-Path $watcomRoot "lib386\nt\user32.lib"
foreach ($library in @($kernelLibrary, $userLibrary)) {
    if (-not (Test-Path -LiteralPath $library)) {
        throw "Required Open Watcom import library was not found: $library"
    }
}

$env:WATCOM = $watcomRoot
$env:Path = "$(Join-Path $watcomRoot 'binnt64');$(Join-Path $watcomRoot 'binnt');$env:Path"
$env:INCLUDE = "$(Join-Path $watcomRoot 'h');$(Join-Path $watcomRoot 'h\nt')"

New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
$source = Join-Path $repoRoot "tools\diag\serial_smoke_win32.c"
$executable = Join-Path $outputDir "v9xser.exe"
$object = Join-Path $outputDir "serial_smoke_win32.obj"
$linkFile = Join-Path $outputDir "v9xser.lnk"
$mapFile = Join-Path $outputDir "v9xser.map"
$compileArguments = @(
    "-bt=nt", "-zq", "-wx", "-zl", "-s",
    "-dV9X_BUILD_ID=`"$BuildId`"",
    "-fo=$object", $source
)

Push-Location $outputDir
try {
    & $compiler @compileArguments
    if ($LASTEXITCODE -ne 0) {
        throw "Open Watcom failed to compile the Win32 serial smoke utility."
    }
}
finally {
    Pop-Location
}

# Link without the Open Watcom C runtime. The default runtime imports Unicode
# startup APIs that the Windows 9x loader does not provide.
$linkDirectives = @(
    "format windows nt",
    "runtime windows=4.0",
    "option quiet",
    "option nodefaultlibs",
    "option start='_V9xSerialEntry@0'",
    "option stack=65536",
    "option map='$mapFile'",
    "name '$executable'",
    "file '$object'",
    "library '$kernelLibrary'",
    "library '$userLibrary'"
)
Set-Content -LiteralPath $linkFile -Value $linkDirectives -Encoding Ascii
& $linker "@$linkFile"
if ($LASTEXITCODE -ne 0) {
    throw "Open Watcom failed to link the runtime-free Win32 serial smoke utility."
}

$bytes = [System.IO.File]::ReadAllBytes($executable)
$newHeaderOffset = if ($bytes.Length -ge 64) {
    [System.BitConverter]::ToInt32($bytes, 0x3c)
} else { -1 }
if ($bytes.Length -lt 64 -or $bytes[0] -ne 0x4d -or $bytes[1] -ne 0x5a -or
    $newHeaderOffset -lt 0 -or $newHeaderOffset + 1 -ge $bytes.Length -or
    $bytes[$newHeaderOffset] -ne 0x50 -or
    $bytes[$newHeaderOffset + 1] -ne 0x45) {
    throw "The Win32 serial smoke output is not an MZ/PE executable."
}
if (-not [System.Text.Encoding]::ASCII.GetString($bytes).Contains($BuildId)) {
    throw "The Win32 serial smoke output does not contain the build identifier."
}

$dumpOutput = @(& $dumper -e $executable 2>&1)
if ($LASTEXITCODE -ne 0) {
    throw "Open Watcom could not inspect the Win32 serial smoke output."
}
$dumpText = $dumpOutput -join "`n"
$expectedImports = @(
    "MessageBoxA",
    "CloseHandle",
    "CreateFileA",
    "ExitProcess",
    "FlushFileBuffers",
    "GetCommState",
    "SetCommState",
    "SetCommTimeouts",
    "WriteFile"
)
foreach ($import in $expectedImports) {
    if ($dumpText -notmatch "(?m)\s$([regex]::Escape($import))\s*$") {
        throw "The Win32 serial smoke output is missing import $import."
    }
}
$dllNames = [regex]::Matches($dumpText, "DLL name = <([^>]+)>") |
    ForEach-Object { $_.Groups[1].Value.ToUpperInvariant() } |
    Sort-Object -Unique
$unexpectedDlls = @($dllNames | Where-Object {
    $_ -notin @("KERNEL32.DLL", "USER32.DLL")
})
if ($unexpectedDlls.Count -ne 0) {
    throw "The runtime-free utility imports unexpected DLLs: $($unexpectedDlls -join ', ')"
}
if ($dumpText -match "GetCommandLineW|GetModuleFileNameW|__CHK") {
    throw "The Win32 serial smoke output contains a forbidden runtime import."
}

Write-Output "Built Win32 COM1 smoke utility: $executable"
Write-Output "Verified runtime-free imports: $($dllNames -join ', ')"
