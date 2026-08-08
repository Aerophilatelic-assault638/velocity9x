[CmdletBinding()]
param(
    [string]$BuildId,
    [string]$DdkRoot = "C:\98DDK"
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$outputDir = Join-Path $repoRoot "build\vm-probe"
$logDir = Join-Path $repoRoot "build\vm-logs"

. (Join-Path $PSScriptRoot "common.ps1")
if (-not $BuildId) {
    $BuildId = Get-V9xBuildId -RepoRoot $repoRoot -Fallback "vm-probe-local"
}

& (Join-Path $PSScriptRoot "build-win32-serial-smoke.ps1") -BuildId $BuildId
& (Join-Path $PSScriptRoot "build-dos-serial-smoke.ps1") -BuildId $BuildId
& (Join-Path $PSScriptRoot "build-vxd-loader-probe.ps1") `
    -BuildId $BuildId -DdkRoot $DdkRoot
& (Join-Path $PSScriptRoot "build-win16-ddi-skeleton.ps1") `
    -BuildId $BuildId -DdkRoot $DdkRoot
& (Join-Path $PSScriptRoot "build-minivdd-skeleton.ps1") `
    -BuildId $BuildId -DdkRoot $DdkRoot

New-Item -ItemType Directory -Force -Path $outputDir,$logDir | Out-Null
Copy-Item -LiteralPath (Join-Path $repoRoot "build\win32-diag\v9xser.exe") `
    -Destination (Join-Path $outputDir "V9XSER.EXE") -Force
Copy-Item -LiteralPath (Join-Path $repoRoot "build\dos-diag\v9xser.exe") `
    -Destination (Join-Path $outputDir "V9XDOS.EXE") -Force
Copy-Item -LiteralPath (Join-Path $repoRoot "build\vxd-probe\v9xvxd.exe") `
    -Destination (Join-Path $outputDir "V9XVXD.EXE") -Force
Copy-Item -LiteralPath (Join-Path $repoRoot "build\vxd-probe\v9xprobe.vxd") `
    -Destination (Join-Path $outputDir "V9XPROBE.VXD") -Force
Copy-Item -LiteralPath (Join-Path $repoRoot "build\win16-ddi\v9xdisp.drv") `
    -Destination (Join-Path $outputDir "V9XDISP.DRV") -Force
Copy-Item -LiteralPath (Join-Path $repoRoot "build\minivdd32\v9xmini.vxd") `
    -Destination (Join-Path $outputDir "V9XMINI.VXD") -Force

$readme = @(
    "Velocity9x VM probe bundle",
    "Build: $BuildId",
    "",
    "SAFE ACTION: run V9XSER.EXE to send one Win32 COM1 smoke line.",
    "V9XDOS.EXE is a direct-UART fallback intended for pure DOS only.",
    "SAFE LIFECYCLE PROBE: run V9XVXD.EXE with V9XPROBE.VXD beside it.",
    "The probe loads, logs, and unloads without touching display hardware.",
    "",
    "DO NOT INSTALL V9XDISP.DRV OR V9XMINI.VXD.",
    "They are ABI/link artifacts whose initialization deliberately fails.",
    "They are included only to prove reproducible transfer into the guest."
)
Set-Content -LiteralPath (Join-Path $outputDir "README.TXT") `
    -Value $readme -Encoding Ascii

$hashLines = Get-ChildItem -LiteralPath $outputDir -File |
    Where-Object { $_.Name -ne "SHA256.TXT" } |
    Sort-Object Name |
    ForEach-Object {
        $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $_.FullName).Hash
        "$hash  $($_.Name)"
    }
Set-Content -LiteralPath (Join-Path $outputDir "SHA256.TXT") `
    -Value $hashLines -Encoding Ascii

Write-Output "Prepared VM probe folder: $outputDir"
Write-Output "Configure COM1 output file: $(Join-Path $logDir 'com1.log')"
Write-Output "For live capture, configure COM1 as named-pipe server velocity9x-com1."
