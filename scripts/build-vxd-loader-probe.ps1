[CmdletBinding()]
param(
    [string]$BuildId,
    [string]$DdkRoot = "C:\98DDK"
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$outputDir = Join-Path $repoRoot "build\vxd-probe"

. (Join-Path $PSScriptRoot "common.ps1")
if (-not $BuildId) {
    $BuildId = Get-V9xBuildId -RepoRoot $repoRoot -Fallback "vxd-probe-local"
}
if ($BuildId -notmatch '^[A-Za-z0-9._+-]+$') {
    throw "BuildId may contain only letters, digits, dot, underscore, plus, and hyphen."
}

$watcomRoot = $env:WATCOM
if (-not $watcomRoot -and (Test-Path -LiteralPath "C:\WATCOM")) {
    $watcomRoot = "C:\WATCOM"
}
if (-not $watcomRoot) {
    throw "Open Watcom was not found. Set WATCOM or install it at C:\WATCOM."
}

$assembler = Join-Path $DdkRoot "bin\win98\ML.EXE"
$vxdLinker = Join-Path $DdkRoot "bin\LINK.EXE"
$ddkInclude = Join-Path $DdkRoot "inc\win98"
$compiler = Join-Path $watcomRoot "binnt64\wcc386.exe"
$peLinker = Join-Path $watcomRoot "binnt64\wlink.exe"
$dumper = Join-Path $watcomRoot "binnt64\wdump.exe"
$kernelLibrary = Join-Path $watcomRoot "lib386\nt\kernel32.lib"
$userLibrary = Join-Path $watcomRoot "lib386\nt\user32.lib"
$requiredInputs = @(
    $assembler, $vxdLinker, (Join-Path $ddkInclude "VMM.INC"),
    $compiler, $peLinker, $dumper, $kernelLibrary, $userLibrary
)
$missingInputs = @($requiredInputs | Where-Object {
    -not (Test-Path -LiteralPath $_)
})
if ($missingInputs.Count -ne 0) {
    throw "Required VxD probe build inputs are missing: $($missingInputs -join ', ')"
}

$env:WATCOM = $watcomRoot
$env:Path = "$(Join-Path $watcomRoot 'binnt64');$(Join-Path $watcomRoot 'binnt');$env:Path"
$env:INCLUDE = "$(Join-Path $watcomRoot 'h');$(Join-Path $watcomRoot 'h\nt')"
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

$buildInclude = Join-Path $outputDir "V9XBUILD.INC"
$definitionFile = Join-Path $outputDir "v9xprobe.def"
$vxdObject = Join-Path $outputDir "vxd_probe.obj"
$vxdPath = Join-Path $outputDir "v9xprobe.vxd"
$vxdMap = Join-Path $outputDir "v9xprobe.map"

Set-Content -LiteralPath $buildInclude -Encoding Ascii -Value @(
    "V9xProbeBuildId db `"velocity9x:$BuildId`", 0",
    "V9xProbeInitLine db `"V9X-VXD init build=$BuildId`", 13, 10",
    "V9xProbeInitLineLength equ `$ - V9xProbeInitLine",
    "V9xProbeVddTableLine db `"V9X-VDD table-ok build=$BuildId`", 13, 10",
    "V9xProbeVddTableLineLength equ `$ - V9xProbeVddTableLine",
    "V9xProbeVddTableFailLine db `"V9X-VDD table-fail build=$BuildId`", 13, 10",
    "V9xProbeVddTableFailLineLength equ `$ - V9xProbeVddTableFailLine",
    "V9xProbeOpenLine db `"V9X-VXD open build=$BuildId`", 13, 10",
    "V9xProbeOpenLineLength equ `$ - V9xProbeOpenLine",
    "V9xProbeCloseLine db `"V9X-VXD close build=$BuildId`", 13, 10",
    "V9xProbeCloseLineLength equ `$ - V9xProbeCloseLine",
    "V9xProbeExitLine db `"V9X-VXD exit build=$BuildId`", 13, 10",
    "V9xProbeExitLineLength equ `$ - V9xProbeExitLine"
)
Set-Content -LiteralPath $definitionFile -Encoding Ascii -Value @(
    "VXD V9XPROBE DYNAMIC",
    "DESCRIPTION 'Velocity9x dynamic VxD lifecycle probe'",
    "SEGMENTS",
    "    _LTEXT CLASS 'LCODE' PRELOAD NONDISCARDABLE",
    "    _LDATA CLASS 'LCODE' PRELOAD NONDISCARDABLE",
    "    _TEXT CLASS 'LCODE' PRELOAD NONDISCARDABLE",
    "    _DATA CLASS 'LCODE' PRELOAD NONDISCARDABLE",
    "    CONST CLASS 'LCODE' PRELOAD NONDISCARDABLE",
    "    _BSS CLASS 'LCODE' PRELOAD NONDISCARDABLE",
    "EXPORTS",
    "    V9XPROBE_DDB @1"
)

$vxdSource = Join-Path $repoRoot "tools\diag\vxd_probe.asm"
$assemblerArguments = @(
    "-coff", "-DBLD_COFF", "-W2", "-Zd", "-c", "-Cx",
    "-DMASM6", "-Sg", "-DVGA", "-DVGA31", "-DMINIVDD=1",
    "-I$ddkInclude", "-I$outputDir",
    "-Fo$vxdObject", $vxdSource
)
& $assembler @assemblerArguments
if ($LASTEXITCODE -ne 0) {
    throw "The Windows 98 DDK assembler failed to build the VxD probe."
}

$vxdLinkerArguments = @(
    "/VXD", "/NOD", $vxdObject, "/IGNORE:4078", "/IGNORE:4039",
    "/OUT:$vxdPath", "/MAP:$vxdMap", "/DEF:$definitionFile"
)
& $vxdLinker @vxdLinkerArguments
if ($LASTEXITCODE -ne 0) {
    throw "The Windows 98 DDK linker failed to create the VxD probe."
}

$vxdBytes = [System.IO.File]::ReadAllBytes($vxdPath)
$vxdHeaderOffset = if ($vxdBytes.Length -ge 64) {
    [System.BitConverter]::ToInt32($vxdBytes, 0x3c)
} else { -1 }
if ($vxdBytes.Length -lt 64 -or $vxdBytes[0] -ne 0x4d -or
    $vxdBytes[1] -ne 0x5a -or $vxdHeaderOffset -lt 0 -or
    $vxdHeaderOffset + 1 -ge $vxdBytes.Length -or
    $vxdBytes[$vxdHeaderOffset] -ne 0x4c -or
    $vxdBytes[$vxdHeaderOffset + 1] -ne 0x45) {
    throw "The VxD probe output is not an MZ/LE image."
}
$vxdText = [System.Text.Encoding]::ASCII.GetString($vxdBytes)
foreach ($marker in @("velocity9x:$BuildId", "V9X-VXD init",
                       "V9X-VDD table-ok", "V9XPROBE_DDB")) {
    if (-not $vxdText.Contains($marker)) {
        throw "The VxD probe output is missing marker $marker."
    }
}

$loaderSource = Join-Path $repoRoot "tools\diag\vxd_probe_win32.c"
$loaderObject = Join-Path $outputDir "vxd_probe_win32.obj"
$loaderPath = Join-Path $outputDir "v9xvxd.exe"
$loaderMap = Join-Path $outputDir "v9xvxd.map"
$loaderLinkFile = Join-Path $outputDir "v9xvxd.lnk"
& $compiler "-bt=nt" "-zq" "-wx" "-zl" "-s" `
    "-fo=$loaderObject" $loaderSource
if ($LASTEXITCODE -ne 0) {
    throw "Open Watcom failed to compile the Win32 VxD loader probe."
}

Set-Content -LiteralPath $loaderLinkFile -Encoding Ascii -Value @(
    "format windows nt",
    "runtime windows=4.0",
    "option quiet",
    "option nodefaultlibs",
    "option start='_V9xVxdProbeEntry@0'",
    "option stack=65536",
    "option map='$loaderMap'",
    "name '$loaderPath'",
    "file '$loaderObject'",
    "library '$kernelLibrary'",
    "library '$userLibrary'"
)
& $peLinker "@$loaderLinkFile"
if ($LASTEXITCODE -ne 0) {
    throw "Open Watcom failed to link the runtime-free Win32 VxD loader probe."
}

$loaderBytes = [System.IO.File]::ReadAllBytes($loaderPath)
$peHeaderOffset = if ($loaderBytes.Length -ge 64) {
    [System.BitConverter]::ToInt32($loaderBytes, 0x3c)
} else { -1 }
if ($loaderBytes.Length -lt 64 -or $loaderBytes[0] -ne 0x4d -or
    $loaderBytes[1] -ne 0x5a -or $peHeaderOffset -lt 0 -or
    $peHeaderOffset + 1 -ge $loaderBytes.Length -or
    $loaderBytes[$peHeaderOffset] -ne 0x50 -or
    $loaderBytes[$peHeaderOffset + 1] -ne 0x45) {
    throw "The VxD loader probe output is not an MZ/PE image."
}

$dumpText = (@(& $dumper -e $loaderPath 2>&1)) -join "`n"
if ($LASTEXITCODE -ne 0) {
    throw "Open Watcom could not inspect the Win32 VxD loader probe."
}
$expectedImports = @(
    "CloseHandle", "CreateFileA", "ExitProcess", "GetLastError", "MessageBoxA"
)
foreach ($import in $expectedImports) {
    if ($dumpText -notmatch "(?m)\s$([regex]::Escape($import))\s*$") {
        throw "The Win32 VxD loader probe is missing import $import."
    }
}
$dllNames = [regex]::Matches($dumpText, "DLL name = <([^>]+)>") |
    ForEach-Object { $_.Groups[1].Value.ToUpperInvariant() } |
    Sort-Object -Unique
$unexpectedDlls = @($dllNames | Where-Object {
    $_ -notin @("KERNEL32.DLL", "USER32.DLL")
})
if ($unexpectedDlls.Count -ne 0 -or
    $dumpText -match "GetCommandLineW|GetModuleFileNameW|__CHK") {
    throw "The Win32 VxD loader probe contains an incompatible runtime import."
}

Write-Output "Built dynamic VxD lifecycle probe: $vxdPath"
Write-Output "Built Win32 VxD load/unload utility: $loaderPath"
Write-Output "Verified runtime-free loader imports: $($dllNames -join ', ')"
