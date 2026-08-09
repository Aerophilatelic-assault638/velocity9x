[CmdletBinding()]
param(
    [string]$BuildId,
    [string]$DdkRoot = "C:\98DDK",
    [ValidateRange(-1, 5)]
    [int]$ForceModeIndex = -1,
    [switch]$BootTrace,
    [switch]$MatroxMillennium2,
    [ValidateSet(8, 16)]
    [int]$MatroxBitsPerPixel = 8
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$outputDir = Join-Path $repoRoot $(if ($MatroxMillennium2) {
    "build\win16-ddi-mga2"
} else {
    "build\win16-ddi"
})

if ($MatroxMillennium2 -and $ForceModeIndex -gt 0) {
    throw "The guarded Millennium II build supports only mode index 0 (640x480x8)."
}

. (Join-Path $PSScriptRoot "common.ps1")
if (-not $BuildId) {
    $BuildId = Get-V9xBuildId -RepoRoot $repoRoot -Fallback "win16-ddi-local"
}

$watcomRoot = $env:WATCOM
if (-not $watcomRoot -and (Test-Path -LiteralPath "C:\WATCOM")) {
    $watcomRoot = "C:\WATCOM"
}
if (-not $watcomRoot) {
    throw "Open Watcom was not found. Set WATCOM or install it at C:\WATCOM."
}

$compiler = Join-Path $watcomRoot "binnt\wcc.exe"
$assembler = Join-Path $watcomRoot "binnt\wasm.exe"
$linker = Join-Path $watcomRoot "binnt\wlink.exe"
$dumper = Join-Path $watcomRoot "binnt64\wdump.exe"
$disassembler = Join-Path $watcomRoot "binnt64\wdis.exe"
$dibEngineLibrary = Join-Path $DdkRoot "lib\win98\DIBENG.LIB"
$runtimeLibrary = Join-Path $watcomRoot "lib286\win\clibc.lib"
$requiredTools = @($compiler, $assembler, $linker, $dumper, $disassembler,
                   $dibEngineLibrary, $runtimeLibrary)
$missingTools = @($requiredTools | Where-Object {
    -not (Test-Path -LiteralPath $_)
})
if ($missingTools.Count -ne 0) {
    throw "Required Win16 build inputs are missing: $($missingTools -join ', ')"
}

$env:WATCOM = $watcomRoot
$env:Path = "$(Join-Path $watcomRoot 'binnt64');$(Join-Path $watcomRoot 'binnt');$env:Path"
$env:INCLUDE = "$(Join-Path $watcomRoot 'h');$(Join-Path $watcomRoot 'h\win')"

New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
$includeDir = Join-Path $repoRoot "include"
$sources = @(
    @{ Name = "build"; Path = "src\common\build.c" },
    @{ Name = "log"; Path = "src\common\log.c" },
    @{ Name = "mode"; Path = "src\common\mode.c" },
    @{ Name = "resources"; Path = "src\common\resources.c" },
    @{ Name = "display_component"; Path = "src\display16\display_component.c" },
    @{ Name = "loader"; Path = "src\display16\loader.c" },
    @{ Name = "ddi"; Path = "src\display16\ddi.c" }
)
if (-not $MatroxMillennium2) {
    $sources = @($sources[0..3]) + @(
        @{ Name = "virge_backend"; Path = "src\chipsets\s3\virge\backend.c" },
        @{ Name = "virge_clocks"; Path = "src\chipsets\s3\virge\clocks.c" }
    ) + @($sources[4..($sources.Count - 1)])
}

foreach ($source in $sources) {
    $sourcePath = Join-Path $repoRoot $source.Path
    $objectPath = Join-Path $outputDir "$($source.Name).obj"
    $arguments = @(
        "-bt=windows", "-mc", "-zu", "-zc", "-zls", "-s", "-zq", "-wx",
        "-i=$includeDir", "-i=$(Join-Path $repoRoot 'src\display16')",
        "-dV9X_BUILD_ID=`"$BuildId`"",
        "-fo=$objectPath", $sourcePath
    )
    # ddi.c already defaults V9X_FORCE_MODE_INDEX to -1. Passing a negative
    # value through wcc's -d option is parsed as another command-line input by
    # this Open Watcom snapshot (E1139), so only define an actual forced mode.
    if ($ForceModeIndex -ge 0) {
        $arguments = @("-dV9X_FORCE_MODE_INDEX=$ForceModeIndex") + $arguments
    }
    if ($BootTrace) {
        $arguments = @("-dV9X_BOOT_TRACE=1") + $arguments
    }
    if ($MatroxMillennium2) {
        $arguments = @("-dV9X_TARGET_MATROX_MILLENNIUM2=1") + $arguments
        if ($MatroxBitsPerPixel -eq 16) {
            $arguments = @("-dV9X_MATROX_16BPP=1") + $arguments
        }
    }
    & $compiler @arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Open Watcom 16-bit compilation failed for $($source.Path)."
    }
}

$thunkSource = Join-Path $repoRoot "src\display16\dib_thunks.asm"
$thunkObject = Join-Path $outputDir "dib_thunks.obj"
& $assembler "-zq" "-fo=$thunkObject" $thunkSource
if ($LASTEXITCODE -ne 0) {
    throw "Open Watcom failed to assemble the DIB Engine forwarding thunks."
}

$runtimeSource = Join-Path $repoRoot "src\display16\runtime.asm"
$runtimeObject = Join-Path $outputDir "runtime.obj"
$runtimeAssemblerArguments = @("-zq", "-fo=$runtimeObject")
if ($MatroxMillennium2) {
    $runtimeAssemblerArguments += "-dV9X_TARGET_MATROX_MILLENNIUM2=1"
    if ($MatroxBitsPerPixel -eq 16) {
        $runtimeAssemblerArguments += "-dV9X_MATROX_16BPP=1"
    }
}
$runtimeAssemblerArguments += $runtimeSource
& $assembler @runtimeAssemblerArguments
if ($LASTEXITCODE -ne 0) {
    throw "Open Watcom failed to assemble the Win16 framebuffer runtime."
}

$driverPath = Join-Path $outputDir "v9xdisp.drv"
$mapPath = Join-Path $outputDir "v9xdisp.map"
$linkFile = Join-Path $outputDir "v9xdisp.lnk"
$objectNames = @($sources.Name) + @("dib_thunks", "runtime")
$linkLines = @(
    "system windows dll initglobal",
    "name '$driverPath'",
    "option start=DriverInit_",
    "option modname=DISPLAY",
    "option map='$mapPath'",
    "option caseexact",
    "option oneautodata",
    "option heapsize=1024",
    "segment type data preload fixed",
    "segment '_TEXT' preload fixed shared"
)
$linkLines += $objectNames | ForEach-Object {
    "file '$(Join-Path $outputDir "$_.obj")'"
}
$linkLines += @(
    "libfile '$dibEngineLibrary'",
    "library '$runtimeLibrary'",
    "reference RESETHIRESMODE",
    "export BitBlt.1", "export ColorInfo.2", "export Control.3",
    "export Disable.4=DISABLE", "export Enable.5=ENABLE", "export EnumDFonts.6",
    "export EnumObj.7", "export Output.8", "export Pixel.9",
    "export RealizeObject.10", "export StrBlt.11", "export ScanLR.12",
    "export DeviceMode.13", "export ExtTextOut.14",
    "export GetCharWidth.15", "export DeviceBitmap.16",
    "export FastBorder.17", "export SetAttribute.18",
    "export DibBlt.19", "export CreateDIBitmap.20",
    "export DibToDevice.21", "export SetPalette.22=SETPALETTE",
    "export GetPalette.23", "export SetPaletteTranslate.24",
    "export GetPaletteTranslate.25", "export UpdateColors.26",
    "export StretchBlt.27", "export StretchDIBits.28",
    "export SelectBitmap.29", "export BitmapBits.30",
    "export ReEnable.31=REENABLE", "export Inquire.101",
    "export SetCursor.102", "export MoveCursor.103",
    "export CheckCursor.104",
    "export ValidateMode.700=VALIDATEMODE"
)
Set-Content -LiteralPath $linkFile -Value $linkLines -Encoding Ascii

& $linker "@$linkFile"
if ($LASTEXITCODE -ne 0) {
    throw "Open Watcom failed to link the Win16 DDI skeleton."
}

$bytes = [System.IO.File]::ReadAllBytes($driverPath)
$newHeaderOffset = if ($bytes.Length -ge 64) {
    [System.BitConverter]::ToInt32($bytes, 0x3c)
} else { -1 }
if ($bytes.Length -lt 64 -or $bytes[0] -ne 0x4d -or $bytes[1] -ne 0x5a -or
    $newHeaderOffset -lt 0 -or $newHeaderOffset + 1 -ge $bytes.Length -or
    $bytes[$newHeaderOffset] -ne 0x4e -or
    $bytes[$newHeaderOffset + 1] -ne 0x45) {
    throw "The Win16 DDI output is not an MZ/NE image."
}
if (-not [System.Text.Encoding]::ASCII.GetString($bytes).Contains($BuildId)) {
    throw "The Win16 DDI output does not contain the build identifier."
}

$exports = (& $dumper "-x" $driverPath 2>&1) -join "`n"
$requiredExports = @("Enable", "Disable", "BitBlt", "ReEnable",
                     "Inquire", "SetCursor", "MoveCursor", "CheckCursor",
                     "ValidateMode")
foreach ($requiredExport in $requiredExports) {
    if ($exports -cnotmatch "(?m)^\s*$requiredExport\s+@") {
        throw "The Win16 DDI output is missing export $requiredExport."
    }
}

$image = (& $dumper "-e" $driverPath 2>&1) -join "`n"
if ($image -notmatch "DIBENG") {
    throw "The Win16 DDI output does not import the DIB Engine."
}
if ($image -notmatch "CODE\|FIXED\|SHARE\|PRELOAD") {
    throw "The Win16 DDI code segment is not fixed, shared, and preloaded."
}
if ($image -notmatch "DATA\|FIXED\|(SHARE\|)?PRELOAD\|READWRITE") {
    throw "The Win16 DDI data segment is not fixed and preloaded."
}
if ($image -notmatch "(?m)^DISPLAY\s+unknown ordinal 0000$") {
    throw "The Win16 DDI internal module name is not DISPLAY."
}
$mapText = Get-Content -LiteralPath $mapPath -Raw
if ($mapText -notmatch "(?m)^.*DriverInit.*$") {
    throw "The Win16 DDI map is missing the DriverInit entry point."
}
foreach ($forbiddenStartup in @('__DLLstart_', 'WINMAIN', 'DEFAULTWINMAIN',
                                 'main_')) {
    if ($mapText -match [regex]::Escape($forbiddenStartup)) {
        throw "The Win16 DDI pulled forbidden C startup symbol $forbiddenStartup."
    }
}
if ($BootTrace) {
    if ($mapText -notmatch "WRITEPRIVATEPROFILESTRING\s+KERNEL") {
        throw "The traced Win16 DDI does not import WritePrivateProfileString."
    }
    $imageText = [System.Text.Encoding]::ASCII.GetString($bytes)
    foreach ($marker in @("libmain", "trace-write-fail stage=libmain")) {
        if (-not $imageText.Contains($marker)) {
            throw "The traced Win16 DDI is missing marker $marker."
        }
    }
}

$requiredRuntimeSymbols = @(
    "V9XHARDWAREPRESENT", "V9XHARDWAREENABLE", "V9XHARDWAREDISABLE",
    "V9XHARDWARESTAGE",
    "V9XVDDGETDISPLAYCONFIG", "V9XVDDPREMODE", "V9XVDDREGISTER", "V9XVDDUNREGISTER",
    "V9XVDDPOSTMODE",
    "V9XCREATEDIBPDEVICECALL", "V9XDIBSETPALETTETRANSLATECALL",
    "DIB_EnumObjExt", "DIB_RealizeObjectExt",
    "DIB_DibBltExt", "DIB_GetPaletteExt", "DIB_SetCursorExt",
    "DIB_MoveCursorExt", "DIB_CheckCursorExt"
)
foreach ($symbol in $requiredRuntimeSymbols) {
    if ($mapText -notmatch "(?m)^.*$([regex]::Escape($symbol)).*$") {
        throw "The Win16 DDI map is missing runtime symbol $symbol."
    }
}

$runtimeDisassembly = (& $disassembler "-a" $runtimeObject 2>&1) -join "`n"
$commonRuntimeInstructions = @(
    'mov\s+eax,80H', 'mov\s+eax,81H', 'mov\s+eax,82H',
    'mov\s+eax,85H', 'mov\s+eax,86H', 'mov\s+eax,87H',
    'push\s+esi', 'push\s+edi',
    'movzx\s+edi,word ptr 6\[bp\]',
    'xor\s+edx,edx',
    'mov\s+ecx,dword ptr DGROUP:_v9x_active_visible_bytes',
    'mov\s+bx,word ptr DGROUP:_v9x_active_vbe_mode',
    'mov\s+ax,seg RESETHIRESMODE', 'int\s+2fH'
)
foreach ($instruction in $commonRuntimeInstructions) {
    if ($runtimeDisassembly -notmatch $instruction) {
        throw "The Win16 runtime is missing audited VDD handoff instruction $instruction."
    }
}
if ($MatroxMillennium2) {
    $matroxPitchInstruction = if ($MatroxBitsPerPixel -eq 16) {
        'cmp\s+bx,500H'
    } else {
        'cmp\s+bx,280H'
    }
    foreach ($instruction in @(
        'or\s+bx,4000H', 'mov\s+cx,51BH', 'mov\s+dx,102BH',
        'mov\s+ax,0B10AH', 'mov\s+di,10H',
        'and\s+eax,0FFFFFFF0H', 'test\s+eax,0FFFFFFH',
        'mov\s+ax,4F06H', 'mov\s+cx,280H', $matroxPitchInstruction
    )) {
        if ($runtimeDisassembly -notmatch $instruction) {
            throw "The Millennium II runtime is missing audited instruction $instruction."
        }
    }
    foreach ($forbiddenInstruction in @(
        'mov\s+cx,8A01H', 'mov\s+dx,5333H',
        'mov\s+al,58H', 'or\s+al,13H',
        'mov\s+di,14H', 'dword ptr es:\[1E54H\]'
    )) {
        if ($runtimeDisassembly -match $forbiddenInstruction) {
            throw "The Millennium II runtime contains forbidden S3 instruction $forbiddenInstruction."
        }
    }
} else {
    foreach ($instruction in @(
        'or\s+bx,8000H', 'mov\s+al,58H', 'and\s+al,0FCH', 'or\s+al,13H'
    )) {
        if ($runtimeDisassembly -notmatch $instruction) {
            throw "The ViRGE/DX runtime is missing audited instruction $instruction."
        }
    }
    if ($runtimeDisassembly -match 'or\s+bx,4000H') {
        throw "The ViRGE/DX runtime must not request the GX2-only VBE LFB flag."
    }
}
foreach ($instruction in @('pop\s+dword ptr .*',
                            'call\s+far ptr CreateDIBPDevice',
                            'push\s+dword ptr .*', 'mov\s+dx,ax',
                            'shr\s+eax,10H', 'xchg\s+ax,dx')) {
    if ($runtimeDisassembly -notmatch $instruction) {
        throw "The CreateDIBPDevice thunk is missing ABI fixup $instruction."
    }
}

$thunkDisassembly = (& $disassembler "-a" $thunkObject 2>&1) -join "`n"
if ($thunkDisassembly -notmatch
    '(?s)CheckCursor:.*?cmp\s+dword ptr es:_v9x_driver_pdevice,0.*?jmp\s+far ptr DIB_CheckCursorExt.*?retf') {
    throw "The DIB CheckCursor thunk is missing its disabled-state guard."
}
if ($thunkDisassembly -notmatch
    '(?s)DibBlt:.*?push\s+word ptr es:_v9x_palettized.*?jmp\s+far ptr DIB_DibBltExt') {
    throw "The DIB BitBlt thunk is not forwarding the selected palette mode."
}

$modeDescription = if ($MatroxMillennium2) {
    "guarded Millennium II 640x480x$MatroxBitsPerPixel"
} else {
    "Phase 3 640/800/1024 x 8/16-bpp"
}
Write-Output "Built $modeDescription Win16 DDI candidate: $driverPath"
