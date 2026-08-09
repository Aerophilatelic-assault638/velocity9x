[CmdletBinding()]
param(
    [string]$BuildId,
    [string]$DdkRoot = "C:\98DDK",
    [ValidateSet(8, 16)]
    [int]$BitsPerPixel = 8
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$outputDir = Join-Path $repoRoot "build\matrox-candidate"

. (Join-Path $PSScriptRoot "common.ps1")
if (-not $BuildId) {
    $BuildId = Get-V9xBuildId -RepoRoot $repoRoot -Fallback "mga2-640x480x8-local"
}
if ($BuildId -notmatch '^[A-Za-z0-9._+-]+$') {
    throw "BuildId may contain only letters, digits, dot, underscore, plus, and hyphen."
}

& (Join-Path $PSScriptRoot "build-win16-ddi-skeleton.ps1") `
    -BuildId $BuildId -DdkRoot $DdkRoot -ForceModeIndex 0 -BootTrace `
    -MatroxMillennium2 -MatroxBitsPerPixel $BitsPerPixel
& (Join-Path $PSScriptRoot "build-minivdd-skeleton.ps1") `
    -BuildId $BuildId -DdkRoot $DdkRoot
& (Join-Path $PSScriptRoot "build-win16-loader-probe.ps1") `
    -BuildId $BuildId -MatroxMillennium2 -MatroxBitsPerPixel $BitsPerPixel
& (Join-Path $PSScriptRoot "build-matrox-recovery.ps1")

New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
Get-ChildItem -LiteralPath $outputDir -File -ErrorAction SilentlyContinue |
    Remove-Item -Force

$driverSource = Join-Path $repoRoot "build\win16-ddi-mga2\v9xdisp.drv"
$miniVddSource = Join-Path $repoRoot "build\minivdd32\v9xmini.vxd"
$loaderSource = Join-Path $repoRoot "build\win16-loader-probe-mga2\v9x16ld.exe"
Copy-Item -LiteralPath $driverSource `
    -Destination (Join-Path $outputDir "MGAPDX64.DRV") -Force
Copy-Item -LiteralPath $miniVddSource `
    -Destination (Join-Path $outputDir "MGAPDX64.VXD") -Force
Copy-Item -LiteralPath $driverSource `
    -Destination (Join-Path $outputDir "V9XDISP.DRV") -Force
Copy-Item -LiteralPath $loaderSource `
    -Destination (Join-Path $outputDir "V9X16LD.EXE") -Force

foreach ($name in @("ACTIVATE.BAT", "ARM.BAT", "DISARM.BAT", "PREPARE.BAT", "README.TXT",
                     "RESTORE.BAT", "V9XGUARD.BAT")) {
    Copy-Item -LiteralPath (Join-Path $repoRoot "build\matrox-recovery\$name") `
        -Destination (Join-Path $outputDir $name) -Force
}

$manifest = @(
    "Velocity9x guarded Matrox Millennium II candidate",
    "Build: $BuildId",
    "Target: PCI 102B:051B only (MGA-2164W)",
    "Mode: forced 640x480x$BitsPerPixel, VBE $(if ($BitsPerPixel -eq 16) {'0111h'} else {'0101h'}), $(640 * ($BitsPerPixel / 8))-byte pitch",
    "Framebuffer: PCI BAR0 / MGABASE2, 4 MiB DPMI mapping",
    "Hardware writes: VBE 4F02h/4F06h only; OPMODE endian swapping remains little-endian",
    "DIB Engine: explicit screen PDevice geometry, pitch, selector and zero surface offset",
    "Installation: guarded stock-file replacement; no INF and no registry edit",
    "Preflight: run V9X16LD.EXE beside V9XDISP.DRV before activation",
    "Rollback: ARM.BAT before copying MGAPDX64.DRV and MGAPDX64.VXD",
    "Success: run DISARM.BAT immediately after a healthy first desktop",
    "Status: HOST-AUDITED; PHYSICAL ACTIVATION NOT YET TESTED"
)
Set-Content -LiteralPath (Join-Path $outputDir "MANIFEST.TXT") `
    -Value $manifest -Encoding Ascii

$forbidden = @(Get-ChildItem -LiteralPath $outputDir -Filter "*.INF" -File)
if ($forbidden.Count -ne 0) {
    throw "The Matrox drop-in candidate must not contain an INF."
}
$driverBytes = [System.IO.File]::ReadAllBytes((Join-Path $outputDir "MGAPDX64.DRV"))
$driverText = [System.Text.Encoding]::ASCII.GetString($driverBytes)
if (-not $driverText.Contains($BuildId) -or
    -not $driverText.Contains("Matrox Millennium II MGA-2164W")) {
    throw "The Matrox display driver is missing its target/build markers."
}
$vxdText = [System.Text.Encoding]::ASCII.GetString(
    [System.IO.File]::ReadAllBytes((Join-Path $outputDir "MGAPDX64.VXD")))
if (-not $vxdText.Contains($BuildId)) {
    throw "The Matrox mini-VDD is missing its build marker."
}

$expected = @(
    "ACTIVATE.BAT", "ARM.BAT", "DISARM.BAT", "MANIFEST.TXT", "MGAPDX64.DRV",
    "MGAPDX64.VXD", "PREPARE.BAT", "README.TXT", "RESTORE.BAT",
    "V9X16LD.EXE", "V9XDISP.DRV", "V9XGUARD.BAT"
)
$actual = @(Get-ChildItem -LiteralPath $outputDir -File |
    ForEach-Object { $_.Name } | Sort-Object)
$missing = @($expected | Where-Object { $_ -notin $actual })
$unexpected = @($actual | Where-Object { $_ -notin $expected })
if ($missing.Count -ne 0 -or $unexpected.Count -ne 0) {
    throw "Matrox package mismatch. Missing: $($missing -join ', '); unexpected: $($unexpected -join ', ')"
}

$hashLines = Get-ChildItem -LiteralPath $outputDir -File |
    Sort-Object Name | ForEach-Object {
        "{0}  {1}" -f (Get-FileHash -Algorithm SHA256 -LiteralPath $_.FullName).Hash,
                       $_.Name
    }
Set-Content -LiteralPath (Join-Path $outputDir "SHA256.TXT") `
    -Value $hashLines -Encoding Ascii

Write-Output "Built host-audited guarded Matrox candidate: $outputDir"
Write-Output "No INF or registry mutation is included."
