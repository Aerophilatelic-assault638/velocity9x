[CmdletBinding()]
param(
    [string]$BuildId,
    [string]$DdkRoot = "C:\98DDK"
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$outputDir = Join-Path $repoRoot "build\win98se-active"

. (Join-Path $PSScriptRoot "common.ps1")
if (-not $BuildId) {
    $BuildId = Get-V9xBuildId -RepoRoot $repoRoot -Fallback "phase3-matrix-local"
}
if ($BuildId -notmatch '^[A-Za-z0-9._+-]+$') {
    throw "BuildId may contain only letters, digits, dot, underscore, plus, and hyphen."
}

& (Join-Path $PSScriptRoot "build-win16-ddi-skeleton.ps1") `
    -BuildId $BuildId -DdkRoot $DdkRoot
& (Join-Path $PSScriptRoot "build-minivdd-skeleton.ps1") `
    -BuildId $BuildId -DdkRoot $DdkRoot
& (Join-Path $PSScriptRoot "build-settings.ps1") -BuildId $BuildId
& (Join-Path $PSScriptRoot "build-gdi-smoke.ps1") -BuildId $BuildId
& (Join-Path $PSScriptRoot "build-vxd-loader-probe.ps1") `
    -BuildId $BuildId -DdkRoot $DdkRoot
& (Join-Path $PSScriptRoot "build-win16-loader-probe.ps1") -BuildId $BuildId
& (Join-Path $PSScriptRoot "build-driver-stage-probe.ps1") -BuildId $BuildId

$infSource = Join-Path $repoRoot "packaging\win98se\velocity9x.inf"
$installSource = Join-Path $repoRoot "packaging\win98se\INSTALL.TXT"
$recoverSource = Join-Path $repoRoot "packaging\win98se\RECOVER.TXT"
$firstBootSource = Join-Path $repoRoot "packaging\win98se\FIRSTBOOT.TXT"
$normalRepairSource = Join-Path $repoRoot "packaging\win98se\V9XFIX.BAT"
$normalRepairInfSource = Join-Path $repoRoot "packaging\win98se\V9XFIX.INF"
$infText = Get-Content -LiteralPath $infSource -Raw

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
foreach ($required in @('v9xdisp.drv', 'v9xmini.vxd',
                         'DEFAULT,Mode,,"8,640,480"',
                         'MODES\8\640,480', 'MODES\8\800,600',
                         'MODES\8\1024,768', 'MODES\16\640,480',
                         'MODES\16\800,600', 'MODES\16\1024,768',
                         'DEFAULT,vdd,,"*vdd,*vflatd"')) {
    if ($infText.IndexOf($required, [StringComparison]::OrdinalIgnoreCase) -lt 0) {
        throw "The active INF is missing required entry $required."
    }
}

New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
Copy-Item -LiteralPath (Join-Path $repoRoot "build\win16-ddi\v9xdisp.drv") `
    -Destination (Join-Path $outputDir "V9XDISP.DRV") -Force
Copy-Item -LiteralPath (Join-Path $repoRoot "build\minivdd32\v9xmini.vxd") `
    -Destination (Join-Path $outputDir "V9XMINI.VXD") -Force
Copy-Item -LiteralPath (Join-Path $repoRoot "build\settings\v9xset.exe") `
    -Destination (Join-Path $outputDir "V9XSET.EXE") -Force
Copy-Item -LiteralPath (Join-Path $repoRoot "build\gdi-smoke\v9xgdi.exe") `
    -Destination (Join-Path $outputDir "V9XGDI.EXE") -Force
Copy-Item -LiteralPath (Join-Path $repoRoot "build\vxd-probe\v9xprobe.vxd") `
    -Destination (Join-Path $outputDir "V9XPROBE.VXD") -Force
Copy-Item -LiteralPath (Join-Path $repoRoot "build\win16-loader-probe\v9x16ld.exe") `
    -Destination (Join-Path $outputDir "V9X16LD.EXE") -Force
Copy-Item -LiteralPath (Join-Path $repoRoot "build\driver-stage-probe\v9xstage.exe") `
    -Destination (Join-Path $outputDir "V9XSTAGE.EXE") -Force
Copy-Item -LiteralPath $infSource `
    -Destination (Join-Path $outputDir "VELOCITY9X.INF") -Force
Copy-Item -LiteralPath $installSource `
    -Destination (Join-Path $outputDir "INSTALL.TXT") -Force
Copy-Item -LiteralPath $recoverSource `
    -Destination (Join-Path $outputDir "RECOVER.TXT") -Force
Copy-Item -LiteralPath $firstBootSource `
    -Destination (Join-Path $outputDir "FIRSTBOOT.TXT") -Force
$normalRepairLines = Get-Content -LiteralPath $normalRepairSource
Set-Content -LiteralPath (Join-Path $outputDir "V9XFIX.BAT") `
    -Value $normalRepairLines -Encoding Ascii
Copy-Item -LiteralPath $normalRepairInfSource `
    -Destination (Join-Path $outputDir "V9XFIX.INF") -Force

$manifest = @(
    "Velocity9x active display bring-up package",
    "Build: $BuildId",
    "Target: Windows 98SE, PCI 5333:8A01 only",
    "Modes: 640x480, 800x600, 1024x768 at 8/16 bpp and 60 Hz",
    "Rendering: Windows DIB Engine, no acceleration",
    "Mini-VDD callbacks: none (master VDD defaults)",
    "Settings: read-only bring-up status, report, and recovery shortcut",
    "GDI test: on-screen primitives, blits, and tolerant pixel readback",
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
    "V9X16LD.EXE", "V9XDISP.DRV", "V9XFIX.BAT", "V9XFIX.INF",
    "V9XGDI.EXE", "V9XMINI.VXD", "V9XPROBE.VXD",
    "V9XSET.EXE", "V9XSTAGE.EXE", "VELOCITY9X.INF"
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
Get-ChildItem -LiteralPath $outputDir -File | ForEach-Object {
    Copy-Item -LiteralPath $_.FullName -Destination $vmStageDir -Force
}

Write-Output "Built host-audited active package: $outputDir"
Write-Output "Staged for the currently mounted folder CD: $vmStageDir"
Write-Output "Guest activation remains blocked on a cold VM disk/NVR backup."
