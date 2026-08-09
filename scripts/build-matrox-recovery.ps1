[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$sourceDir = Join-Path $repoRoot "packaging\win98se\matrox-recovery"
$outputDir = Join-Path $repoRoot "build\matrox-recovery"
$expected = @("ARM.BAT", "DISARM.BAT", "PREPARE.BAT", "README.TXT",
              "RESTORE.BAT", "V9XGUARD.BAT")

New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
Get-ChildItem -LiteralPath $outputDir -File -ErrorAction SilentlyContinue |
    Remove-Item -Force
foreach ($name in $expected) {
    $source = Join-Path $sourceDir $name
    if (-not (Test-Path -LiteralPath $source)) { throw "Missing recovery input $name" }
    $lines = Get-Content -LiteralPath $source
    Set-Content -LiteralPath (Join-Path $outputDir $name) -Value $lines -Encoding Ascii
}

$guard = Get-Content -LiteralPath (Join-Path $outputDir "V9XGUARD.BAT") -Raw
foreach ($required in @("ARMED.1", "ARMED.2", "MGAPDX64.DRV",
                         "MGAPDX64.VXD", "ROLLED-BACK")) {
    if ($guard.IndexOf($required, [StringComparison]::OrdinalIgnoreCase) -lt 0) {
        throw "Recovery guard is missing $required"
    }
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
