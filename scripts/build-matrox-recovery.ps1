[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$sourceDir = Join-Path $repoRoot "packaging\win98se\matrox-recovery"
$outputDir = Join-Path $repoRoot "build\matrox-recovery"
$expected = @("ACTIVATE.BAT", "ARM.BAT", "DISARM.BAT", "PREPARE.BAT", "README.TXT",
              "RESTORE.BAT", "V9XAUTO.EXE", "V9XGUARD.BAT", "V9XSETP.REG")

& (Join-Path $PSScriptRoot "build-matrox-guard-autoexec.ps1")

New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
Get-ChildItem -LiteralPath $outputDir -File -ErrorAction SilentlyContinue |
    Remove-Item -Force
foreach ($name in $expected) {
    if ($name -eq "V9XAUTO.EXE") {
        Copy-Item -LiteralPath (Join-Path $repoRoot "build\matrox-guard-autoexec\v9xauto.exe") `
            -Destination (Join-Path $outputDir $name) -Force
        continue
    }
    $source = Join-Path $sourceDir $name
    if (-not (Test-Path -LiteralPath $source)) { throw "Missing recovery input $name" }
    $lines = Get-Content -LiteralPath $source
    Set-Content -LiteralPath (Join-Path $outputDir $name) -Value $lines -Encoding Ascii
}

$guard = Get-Content -LiteralPath (Join-Path $outputDir "V9XGUARD.BAT") -Raw
foreach ($required in @("ARMED.1", "ARMED.2", "MGAPDX64.DRV",
                         "MGAPDX64.VXD", "CANDDRV.DRV", "CANDVXD.VXD",
                         "ROLLED-BACK")) {
    if ($guard.IndexOf($required, [StringComparison]::OrdinalIgnoreCase) -lt 0) {
        throw "Recovery guard is missing $required"
    }
}
$activate = Get-Content -LiteralPath (Join-Path $outputDir "ACTIVATE.BAT") -Raw
foreach ($required in @("FC /B", "ARM.BAT", "DISARM.BAT", "MGAPDX64.DRV",
                         "MGAPDX64.VXD", "KEEPVXD.TAG", "CANDVXD.VXD",
                         "V9XSETP.DLL", "V9XSETP.REG", "CANDSET.DLL")) {
    if ($activate.IndexOf($required, [StringComparison]::OrdinalIgnoreCase) -lt 0) {
        throw "Recovery activation helper is missing $required"
    }
}
$disarm = Get-Content -LiteralPath (Join-Path $outputDir "DISARM.BAT") -Raw
foreach ($required in @("NOSET", "CANDSET.DLL", "CANDSET.REG", "REGEDIT /S",
                         "V9XSETP.DLL")) {
    if ($disarm.IndexOf($required, [StringComparison]::OrdinalIgnoreCase) -lt 0) {
        throw "Recovery disarm helper is missing settings-page step $required"
    }
}
$prepare = Get-Content -LiteralPath (Join-Path $outputDir "PREPARE.BAT") -Raw
foreach ($required in @("V9XAUTO.EXE", "AUTOEXEC.BAK")) {
    if ($activate.IndexOf($required, [StringComparison]::OrdinalIgnoreCase) -ge 0) {
        throw "Activation helper unexpectedly owns AUTOEXEC setup: $required"
    }
    if ($prepare.IndexOf($required, [StringComparison]::OrdinalIgnoreCase) -lt 0) {
        throw "Recovery preparation is missing $required"
    }
}
$restore = Get-Content -LiteralPath (Join-Path $outputDir "RESTORE.BAT") -Raw
foreach ($required in @("ARMED.2", "RESTORE-PENDING", "Reboot Windows exactly once")) {
    if ($restore.IndexOf($required, [StringComparison]::OrdinalIgnoreCase) -lt 0) {
        throw "Recovery restore helper is missing $required"
    }
}
if ($restore.IndexOf("COPY /Y", [StringComparison]::OrdinalIgnoreCase) -ge 0) {
    throw "Recovery restore helper must defer loaded-file copies to DOS time."
}
if ($guard -match '(?i)REGEDIT|FORMAT|FDISK|DELTREE') {
    throw "Recovery guard contains an out-of-scope command."
}

$hashLines = Get-ChildItem -LiteralPath $outputDir -File |
    Sort-Object Name | ForEach-Object {
        "{0}  {1}" -f (Get-FileHash -Algorithm SHA256 -LiteralPath $_.FullName).Hash,
                       $_.Name
    }
Set-Content -LiteralPath (Join-Path $outputDir "SHA256.TXT") `
    -Value $hashLines -Encoding Ascii
Write-Output "Built inert Matrox recovery guard: $outputDir"
