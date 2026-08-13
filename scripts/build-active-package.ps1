[CmdletBinding()]
param(
    [string]$BuildId,
    [string]$DdkRoot = "C:\98DDK",
    [ValidateRange(-1, 5)]
    [int]$ForceModeIndex = -1,
    [switch]$BootTrace
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$outputDir = Join-Path $repoRoot "build\win98se-active"

. (Join-Path $PSScriptRoot "common.ps1")
$ProductVersion = Get-V9xProductVersion -RepoRoot $repoRoot
if (-not $BuildId) {
    $BuildId = Get-V9xBuildId -RepoRoot $repoRoot -Fallback "phase3-matrix-local"
}
if ($BuildId -notmatch '^[A-Za-z0-9._+-]+$') {
    throw "BuildId may contain only letters, digits, dot, underscore, plus, and hyphen."
}

& (Join-Path $PSScriptRoot "build-win16-ddi-skeleton.ps1") `
    -BuildId $BuildId -DdkRoot $DdkRoot -ForceModeIndex $ForceModeIndex `
    -BootTrace:$BootTrace
& (Join-Path $PSScriptRoot "build-minivdd-skeleton.ps1") `
    -BuildId $BuildId -DdkRoot $DdkRoot
& (Join-Path $PSScriptRoot "build-settings.ps1") -BuildId $BuildId
& (Join-Path $PSScriptRoot "build-settings-page.ps1") -BuildId $BuildId
& (Join-Path $PSScriptRoot "build-gdi-smoke.ps1") -BuildId $BuildId
& (Join-Path $PSScriptRoot "build-palette-smoke.ps1") -BuildId $BuildId
& (Join-Path $PSScriptRoot "build-mode-switch.ps1") -BuildId $BuildId
& (Join-Path $PSScriptRoot "build-power-cycle.ps1") -BuildId $BuildId
& (Join-Path $PSScriptRoot "build-ddraw-probe.ps1") -BuildId $BuildId
& (Join-Path $PSScriptRoot "build-trace-dump.ps1") -BuildId $BuildId
& (Join-Path $PSScriptRoot "build-window-list.ps1") -BuildId $BuildId
& (Join-Path $PSScriptRoot "build-ddraw-hal-dll.ps1") -BuildId $BuildId
& (Join-Path $PSScriptRoot "build-vxd-loader-probe.ps1") `
    -BuildId $BuildId -DdkRoot $DdkRoot
& (Join-Path $PSScriptRoot "build-win16-loader-probe.ps1") -BuildId $BuildId
& (Join-Path $PSScriptRoot "build-driver-stage-probe.ps1") -BuildId $BuildId

$infSource = Join-Path $repoRoot "packaging\win98se\velocity9x.inf"
$installSource = Join-Path $repoRoot "packaging\win98se\INSTALL.TXT"
$recoverSource = Join-Path $repoRoot "packaging\win98se\RECOVER.TXT"
$firstBootSource = Join-Path $repoRoot "packaging\win98se\FIRSTBOOT.TXT"
$normalRepairSource = Join-Path $repoRoot "packaging\win98se\V9XFIX.BAT"
$infText = Get-Content -LiteralPath $infSource -Raw
$forcedModes = @(
    "8,640,480", "8,800,600", "8,1024,768",
    "16,640,480", "16,800,600", "16,1024,768"
)
$defaultMode = if ($ForceModeIndex -ge 0) {
    $forcedModes[$ForceModeIndex]
} else {
    "8,640,480"
}
$infText = $infText.Replace('DEFAULT,Mode,,"8,640,480"',
                            "DEFAULT,Mode,,`"$defaultMode`"")
$infText = $infText.Replace('Provider=%Provider%',
                            'Provider="Velocity9x Project"')
$infText = $infText.Replace('1=%DiskName%,,0',
                            '1="Velocity9x Windows 98SE driver-stage disk",,0')
$infText = $infText.Replace('%Manufacturer%=Velocity9x.Models',
                            'Velocity9x=Velocity9x.Models')
$infText = $infText.Replace('%DeviceDesc%=Velocity9x.Install',
                            '"Velocity9x S3 ViRGE/DX 86C375 (Phase 3 mode matrix)"=Velocity9x.Install')
if ($infText -match '%[A-Za-z][A-Za-z0-9_]*%') {
    throw "The generated active INF contains an unresolved string token."
}

$hardwareIds = @([regex]::Matches(
    $infText, 'PCI\\VEN_[0-9A-Fa-f]{4}&DEV_[0-9A-Fa-f]{4}') |
    ForEach-Object { $_.Value.ToUpperInvariant() } | Sort-Object -Unique)
if ($hardwareIds.Count -ne 1 -or
    $hardwareIds[0] -ne 'PCI\VEN_5333&DEV_8A01') {
    throw "The active INF must match only PCI\VEN_5333&DEV_8A01."
}
foreach ($forbidden in @('MODES\24',
                          'MODES\32', 'DDC', 'carddvdd')) {
    if ($infText.IndexOf($forbidden, [StringComparison]::OrdinalIgnoreCase) -ge 0) {
        throw "The active INF contains out-of-scope entry $forbidden."
    }
}
foreach ($required in @('v9xdisp.drv', 'v9xmini.vxd', 'v9xhal.dll', 'v9xsetp.dll',
                         'Controls Folder\Display\shellex\PropertySheetHandlers\Velocity9x',
                         'CLSID\{91925DA2-2EF0-4E20-B4E9-A53ED37E14B1}\InProcServer32',
                         "DEFAULT,Mode,,`"$defaultMode`"",
                         'MODES\8\640,480', 'MODES\8\800,600',
                         'MODES\8\1024,768', 'MODES\16\640,480',
                         'MODES\16\800,600', 'MODES\16\1024,768',
                         'DEFAULT,vdd,,"*vdd,*vflatd"',
                         'DEFAULT,RefreshRate,,0',
                         'DEFAULT,PCIRebalance,,1')) {
    if ($infText.IndexOf($required, [StringComparison]::OrdinalIgnoreCase) -lt 0) {
        throw "The active INF is missing required entry $required."
    }
}
if ($infText -match '(?im)^HKR,CURRENT,') {
    throw "The active INF must let Windows create the volatile CURRENT display key."
}

New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
Remove-Item -LiteralPath (Join-Path $outputDir "V9XFIX.INF") -Force `
    -ErrorAction SilentlyContinue
Copy-Item -LiteralPath (Join-Path $repoRoot "build\win16-ddi\v9xdisp.drv") `
    -Destination (Join-Path $outputDir "V9XDISP.DRV") -Force
Copy-Item -LiteralPath (Join-Path $repoRoot "build\minivdd32\v9xmini.vxd") `
    -Destination (Join-Path $outputDir "V9XMINI.VXD") -Force
Copy-Item -LiteralPath (Join-Path $repoRoot "build\settings\v9xset.exe") `
    -Destination (Join-Path $outputDir "V9XSET.EXE") -Force
Copy-Item -LiteralPath (Join-Path $repoRoot "build\settings-page\v9xsetp.dll") `
    -Destination (Join-Path $outputDir "V9XSETP.DLL") -Force
Copy-Item -LiteralPath (Join-Path $repoRoot "build\ddraw-hal\v9xhal.dll") `
    -Destination (Join-Path $outputDir "V9XHAL.DLL") -Force
Copy-Item -LiteralPath (Join-Path $repoRoot "build\gdi-smoke\v9xgdi.exe") `
    -Destination (Join-Path $outputDir "V9XGDI.EXE") -Force
Copy-Item -LiteralPath (Join-Path $repoRoot "build\palette-smoke\v9xpal.exe") `
    -Destination (Join-Path $outputDir "V9XPAL.EXE") -Force
Copy-Item -LiteralPath (Join-Path $repoRoot "build\mode-switch\v9xmsw.exe") `
    -Destination (Join-Path $outputDir "V9XMSW.EXE") -Force
Copy-Item -LiteralPath (Join-Path $repoRoot "build\power-cycle\v9xpwr.exe") `
    -Destination (Join-Path $outputDir "V9XPWR.EXE") -Force
Copy-Item -LiteralPath (Join-Path $repoRoot "build\ddraw-probe\v9xddp.exe") `
    -Destination (Join-Path $outputDir "V9XDDP.EXE") -Force
Copy-Item -LiteralPath (Join-Path $repoRoot "build\trace-dump\v9xtrace.exe") `
    -Destination (Join-Path $outputDir "V9XTRACE.EXE") -Force
Copy-Item -LiteralPath (Join-Path $repoRoot "build\window-list\v9xwnd.exe") `
    -Destination (Join-Path $outputDir "V9XWND.EXE") -Force
Copy-Item -LiteralPath (Join-Path $repoRoot "build\vxd-probe\v9xprobe.vxd") `
    -Destination (Join-Path $outputDir "V9XPROBE.VXD") -Force
Copy-Item -LiteralPath (Join-Path $repoRoot "build\win16-loader-probe\v9x16ld.exe") `
    -Destination (Join-Path $outputDir "V9X16LD.EXE") -Force
Copy-Item -LiteralPath (Join-Path $repoRoot "build\driver-stage-probe\v9xstage.exe") `
    -Destination (Join-Path $outputDir "V9XSTAGE.EXE") -Force
$infLines = [regex]::Split($infText.TrimEnd("`r", "`n"), "\r?\n")
Set-Content -LiteralPath (Join-Path $outputDir "VELOCITY9X.INF") `
    -Value $infLines -Encoding Ascii
Copy-Item -LiteralPath $installSource `
    -Destination (Join-Path $outputDir "INSTALL.TXT") -Force
Copy-Item -LiteralPath $recoverSource `
    -Destination (Join-Path $outputDir "RECOVER.TXT") -Force
Copy-Item -LiteralPath $firstBootSource `
    -Destination (Join-Path $outputDir "FIRSTBOOT.TXT") -Force
$normalRepairLines = Get-Content -LiteralPath $normalRepairSource
Set-Content -LiteralPath (Join-Path $outputDir "V9XFIX.BAT") `
    -Value $normalRepairLines -Encoding Ascii

$manifest = @(
    "Velocity9x active display bring-up package",
    "Version: $ProductVersion",
    "Build: $BuildId",
    "Target: Windows 98SE, PCI 5333:8A01 only",
    "Modes: 640x480, 800x600, 1024x768 at 8/16 bpp and 60 Hz",
    "Forced diagnostic mode index: $ForceModeIndex (-1 means registry-selected)",
    "Boot trace: $BootTrace (writes C:\\V9XBOOT.INI)",
    "Rendering: Windows DIB Engine, no acceleration",
    "Mini-VDD callbacks: D0-only caps + guarded VESA DPMS + Win98 power state",
    "Settings: read-only bring-up status, report, and recovery shortcut",
    "Display Properties: read-only Velocity9x tab via V9XSETP.DLL",
    "GDI test: on-screen primitives, blits, and tolerant pixel readback",
    "Palette test: 8-bit reserved-entry animation and screen readback",
    "Mode switching: live same-depth via ReEnable; depth change needs restart",
    "DirectDraw HAL: V9XHAL.DLL (vidmem + flip + bounded solid fill)",
    "Mode-switch test: V9XMSW.EXE (/set:WxHxB, /cycle:N, /depth)",
    "Monitor-power test: V9XPWR.EXE (D3 off, then D0 wake)",
    "DirectDraw probe: V9XDDP.EXE (flip timing and mode honesty)",
    "HAL trace: driver writes C:\\V9XTRACE.INI on faults; V9XTRACE.EXE writes live C:\\V9XSNAP.INI",
    "Window inventory: V9XWND.EXE writes GDI-free C:\\V9XWND.INI",
    "Preflight: V9XSTAGE.EXE (no mode change and no installation)",
    "Status: HOST-AUDITED; GUEST ACTIVATION NOT YET TESTED",
    "",
    "Read FIRSTBOOT.TXT, INSTALL.TXT, and RECOVER.TXT before selecting the INF."
)
Set-Content -LiteralPath (Join-Path $outputDir "MANIFEST.TXT") `
    -Encoding Ascii -Value $manifest

$hashLines = Get-ChildItem -LiteralPath $outputDir -File |
    Where-Object { $_.Name -ne "SHA256.TXT" } |
    Sort-Object Name |
    ForEach-Object {
        $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $_.FullName).Hash
        "$hash  $($_.Name)"
    }
Set-Content -LiteralPath (Join-Path $outputDir "SHA256.TXT") `
    -Encoding Ascii -Value $hashLines

$expectedPackageFiles = @(
    "FIRSTBOOT.TXT", "INSTALL.TXT", "MANIFEST.TXT", "RECOVER.TXT", "SHA256.TXT",
    "V9X16LD.EXE", "V9XDDP.EXE", "V9XDISP.DRV", "V9XFIX.BAT", "V9XHAL.DLL",
    "V9XGDI.EXE", "V9XMSW.EXE", "V9XPAL.EXE", "V9XPWR.EXE",
    "V9XMINI.VXD", "V9XPROBE.VXD",
    "V9XSET.EXE", "V9XSETP.DLL", "V9XSTAGE.EXE", "V9XTRACE.EXE",
    "V9XWND.EXE",
    "VELOCITY9X.INF"
)
$actualPackageFiles = @(Get-ChildItem -LiteralPath $outputDir -File |
    ForEach-Object { $_.Name } | Sort-Object)
$unexpectedPackageFiles = @($actualPackageFiles |
    Where-Object { $_ -notin $expectedPackageFiles })
$missingPackageFiles = @($expectedPackageFiles |
    Where-Object { $_ -notin $actualPackageFiles })
if ($unexpectedPackageFiles.Count -ne 0 -or $missingPackageFiles.Count -ne 0) {
    throw "Active package file-set mismatch. Missing: $($missingPackageFiles -join ', '); unexpected: $($unexpectedPackageFiles -join ', ')"
}

$vmStageDir = Join-Path $repoRoot "build\vm-probe\ACTIVE"
New-Item -ItemType Directory -Force -Path $vmStageDir | Out-Null
Remove-Item -LiteralPath (Join-Path $vmStageDir "V9XFIX.INF") -Force `
    -ErrorAction SilentlyContinue
Get-ChildItem -LiteralPath $outputDir -File | ForEach-Object {
    Copy-Item -LiteralPath $_.FullName -Destination $vmStageDir -Force
}

Write-Output "Built host-audited active package: $outputDir"
Write-Output "Staged for the currently mounted folder CD: $vmStageDir"
Write-Output "Guest activation remains blocked on a cold VM disk/NVR backup."
