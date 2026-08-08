[CmdletBinding()]
param(
    [string]$BuildId = "win16-local"
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$outputDir = Join-Path $repoRoot "build\win16"

$watcomRoot = $env:WATCOM
if (-not $watcomRoot -and (Test-Path -LiteralPath "C:\WATCOM")) {
    $watcomRoot = "C:\WATCOM"
}
if (-not $watcomRoot) {
    throw "Open Watcom was not found. Set WATCOM or install it at C:\WATCOM."
}

$compiler = Join-Path $watcomRoot "binnt\wcc.exe"
$linker = Join-Path $watcomRoot "binnt\wlink.exe"
if (-not (Test-Path -LiteralPath $compiler) -or
    -not (Test-Path -LiteralPath $linker)) {
    throw "The Open Watcom 16-bit compiler or linker is missing under $watcomRoot."
}

$env:WATCOM = $watcomRoot
$env:Path = "$(Join-Path $watcomRoot 'binnt64');$(Join-Path $watcomRoot 'binnt');$env:Path"
$env:INCLUDE = "$(Join-Path $watcomRoot 'h');$(Join-Path $watcomRoot 'h\win')"

New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
$includeDir = Join-Path $repoRoot "include"
$sources = @(
    @{ Name = "build"; Path = "src\common\build.c" },
    @{ Name = "log"; Path = "src\common\log.c" },
    @{ Name = "display_component"; Path = "src\display16\display_component.c" },
    @{ Name = "loader"; Path = "src\display16\loader.c" }
)

foreach ($source in $sources) {
    $sourcePath = Join-Path $repoRoot $source.Path
    $objectPath = Join-Path $outputDir "$($source.Name).obj"
    $arguments = @(
        "-bt=windows",
        "-mc",
        "-zu",
        "-zc",
        "-bd",
        "-zq",
        "-wx",
        "-i=$includeDir",
        "-dV9X_BUILD_ID=`"$BuildId`"",
        "-fo=$objectPath",
        $sourcePath
    )
    & $compiler @arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Open Watcom 16-bit compilation failed for $($source.Path)."
    }
}

$driverPath = Join-Path $outputDir "v9xdisp.drv"
$mapPath = Join-Path $outputDir "v9xdisp.map"
$linkFile = Join-Path $outputDir "v9xdisp.lnk"
$linkLines = @(
    "system windows dll initinstance memory",
    "name '$driverPath'",
    "option map='$mapPath'",
    "option oneautodata",
    "option heapsize=1024",
    "file '$(Join-Path $outputDir 'build.obj')'",
    "file '$(Join-Path $outputDir 'log.obj')'",
    "file '$(Join-Path $outputDir 'display_component.obj')'",
    "file '$(Join-Path $outputDir 'loader.obj')'",
    "libfile libentry",
    "export WEP RESIDENT"
)
Set-Content -LiteralPath $linkFile -Value $linkLines -Encoding Ascii

& $linker "@$linkFile"
if ($LASTEXITCODE -ne 0) {
    throw "Open Watcom failed to link the Win16 loader shell."
}

$bytes = [System.IO.File]::ReadAllBytes($driverPath)
if ($bytes.Length -lt 64 -or $bytes[0] -ne 0x4d -or $bytes[1] -ne 0x5a) {
    throw "The Win16 output does not have an MZ executable header."
}
$newHeaderOffset = [System.BitConverter]::ToInt32($bytes, 0x3c)
if ($newHeaderOffset -lt 0 -or $newHeaderOffset + 1 -ge $bytes.Length -or
    $bytes[$newHeaderOffset] -ne 0x4e -or
    $bytes[$newHeaderOffset + 1] -ne 0x45) {
    throw "The Win16 output does not have an NE header."
}
$imageText = [System.Text.Encoding]::ASCII.GetString($bytes)
if (-not $imageText.Contains($BuildId)) {
    throw "The Win16 output does not contain the requested build identifier."
}
$wepExport = Select-String -LiteralPath $mapPath -Pattern '^.{0,20}WEP$'
if (-not $wepExport) {
    throw "The Win16 link map does not contain the WEP export."
}

Write-Output "Built Win16 NE loader shell: $driverPath"
