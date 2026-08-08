[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$required = @(
    "PLAN.md",
    "README.md",
    "docs\vm-environment.md",
    "docs\decisions\2026-08-08-vxd-lifecycle-probe.md",
    "docs\specifications\win9x-driver-boundaries.md",
    "docs\specifications\logging-protocol.md",
    "include\velocity9x\backend.h",
    "scripts\common.ps1",
    "scripts\build-host.ps1",
    "scripts\build-host-msvc.ps1",
    "scripts\build-win16-skeleton.ps1",
    "scripts\build-win16-ddi-skeleton.ps1",
    "scripts\build-minivdd-skeleton.ps1",
    "scripts\build-dos-serial-smoke.ps1",
    "scripts\build-win32-serial-smoke.ps1",
    "scripts\build-vxd-loader-probe.ps1",
    "scripts\capture-serial-pipe.ps1",
    "scripts\prepare-vm-probe.ps1",
    "src\common\mode.c",
    "src\common\resources.c",
    "src\chipsets\s3\virge\backend.c",
    "src\display16\display_component.c",
    "src\display16\loader.c",
    "src\display16\ddi.c",
    "src\display16\dib_thunks.asm",
    "src\minivdd32\minivdd_component.c",
    "src\minivdd32\loader.asm",
    "tests\host\test_main.c",
    "tools\diag\serial_smoke.c",
    "tools\diag\serial_smoke_win32.c",
    "tools\diag\vxd_probe.asm",
    "tools\diag\vxd_probe_win32.c"
)

$missing = @($required | Where-Object {
    -not (Test-Path -LiteralPath (Join-Path $repoRoot $_))
})
if ($missing.Count -ne 0) {
    throw "Required repository files are missing: $($missing -join ', ')"
}

$sourceFiles = Get-ChildItem -LiteralPath (Join-Path $repoRoot "src") -Recurse -File
$sourceFiles += Get-ChildItem -LiteralPath (Join-Path $repoRoot "include") -Recurse -File
$allowedOsBoundaries = @(
    (Join-Path $repoRoot "src\display16\loader.c"),
    (Join-Path $repoRoot "src\display16\ddi.c"),
    (Join-Path $repoRoot "src\minivdd32\loader.asm")
)
$forbidden = $sourceFiles |
    Select-String -Pattern '#include\s*[<"]windows\.h[>"]|#include\s*[<"]vmm\.h[>"]|include\s+(VMM|MINIVDD)\.INC' |
    Where-Object { $_.Path -notin $allowedOsBoundaries }
if ($forbidden) {
    $forbidden | ForEach-Object { Write-Error $_.ToString() }
    throw "Portable skeleton source contains an unapproved Windows/DDK dependency."
}

Write-Output "Velocity9x tree check passed ($($sourceFiles.Count) source/header files)."
