[CmdletBinding()]
param(
    [string]$BuildId,
    [string]$DdkRoot = "C:\98DDK"
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$outputDir = Join-Path $repoRoot "build\minivdd32"

. (Join-Path $PSScriptRoot "common.ps1")
if (-not $BuildId) {
    $BuildId = Get-V9xBuildId -RepoRoot $repoRoot -Fallback "minivdd-local"
}
if ($BuildId -notmatch '^[A-Za-z0-9._+-]+$') {
    throw "BuildId may contain only letters, digits, dot, underscore, plus, and hyphen."
}

$assembler = Join-Path $DdkRoot "bin\win98\ML.EXE"
$linker = Join-Path $DdkRoot "bin\LINK.EXE"
$ddkInclude = Join-Path $DdkRoot "inc\win98"
$requiredInputs = @($assembler, $linker,
                    (Join-Path $ddkInclude "VMM.INC"),
                    (Join-Path $ddkInclude "MINIVDD.INC"))
$missingInputs = @($requiredInputs | Where-Object {
    -not (Test-Path -LiteralPath $_)
})
if ($missingInputs.Count -ne 0) {
    throw "Required Windows 98 DDK inputs are missing: $($missingInputs -join ', ')"
}

New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
$buildInclude = Join-Path $outputDir "V9XBUILD.INC"
$definitionFile = Join-Path $outputDir "v9xmini.def"
$objectPath = Join-Path $outputDir "loader.obj"
$vxdPath = Join-Path $outputDir "v9xmini.vxd"
$mapPath = Join-Path $outputDir "v9xmini.map"

Set-Content -LiteralPath $buildInclude -Encoding Ascii -Value `
    "db `"velocity9x:$BuildId`", 0"
Set-Content -LiteralPath $definitionFile -Encoding Ascii -Value @(
    "VXD V9XMINI DYNAMIC",
    "DESCRIPTION 'Velocity9x inert mini-VDD ABI skeleton'",
    "SEGMENTS",
    "    _LTEXT CLASS 'LCODE' PRELOAD NONDISCARDABLE",
    "    _LDATA CLASS 'LCODE' PRELOAD NONDISCARDABLE",
    "    _TEXT CLASS 'LCODE' PRELOAD NONDISCARDABLE",
    "    _DATA CLASS 'LCODE' PRELOAD NONDISCARDABLE",
    "    CONST CLASS 'LCODE' PRELOAD NONDISCARDABLE",
    "    _BSS CLASS 'LCODE' PRELOAD NONDISCARDABLE",
    "    _ITEXT CLASS 'ICODE' DISCARDABLE",
    "    _IDATA CLASS 'ICODE' DISCARDABLE",
    "EXPORTS",
    "    V9XMINI_DDB @1"
)

$sourcePath = Join-Path $repoRoot "src\minivdd32\loader.asm"
$assemblerArguments = @(
    "-coff", "-DBLD_COFF", "-W2", "-Zd", "-c", "-Cx",
    "-DMASM6", "-Sg", "-DVGA", "-DVGA31", "-DMINIVDD=1",
    "-I$ddkInclude", "-I$outputDir", "-Fo$objectPath", $sourcePath
)
& $assembler @assemblerArguments
if ($LASTEXITCODE -ne 0) {
    throw "The Windows 98 DDK assembler failed to build the mini-VDD skeleton."
}

$linkerArguments = @(
    "/VXD", "/NOD", $objectPath, "/IGNORE:4078", "/IGNORE:4039",
    "/OUT:$vxdPath", "/MAP:$mapPath", "/DEF:$definitionFile"
)
& $linker @linkerArguments
if ($LASTEXITCODE -ne 0) {
    throw "The Windows 98 DDK linker failed to create the mini-VDD skeleton."
}

$bytes = [System.IO.File]::ReadAllBytes($vxdPath)
$newHeaderOffset = if ($bytes.Length -ge 64) {
    [System.BitConverter]::ToInt32($bytes, 0x3c)
} else { -1 }
if ($bytes.Length -lt 64 -or $bytes[0] -ne 0x4d -or $bytes[1] -ne 0x5a -or
    $newHeaderOffset -lt 0 -or $newHeaderOffset + 1 -ge $bytes.Length -or
    $bytes[$newHeaderOffset] -ne 0x4c -or
    $bytes[$newHeaderOffset + 1] -ne 0x45) {
    throw "The mini-VDD output is not an MZ/LE image."
}

$imageText = [System.Text.Encoding]::ASCII.GetString($bytes)
if (-not $imageText.Contains("velocity9x:$BuildId")) {
    throw "The mini-VDD output does not contain the build identifier."
}
if (-not $imageText.Contains("V9XMINI_DDB")) {
    throw "The mini-VDD output does not export V9XMINI_DDB."
}

Write-Output "Built inert mini-VDD ABI skeleton: $vxdPath"
