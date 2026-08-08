[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$required = @(
    "PLAN.md",
    "README.md",
    "docs\specifications\win9x-driver-boundaries.md",
    "docs\specifications\logging-protocol.md",
    "include\velocity9x\backend.h",
    "scripts\common.ps1",
    "scripts\build-host.ps1",
    "scripts\build-host-msvc.ps1",
    "scripts\build-win16-skeleton.ps1",
    "src\common\mode.c",
    "src\chipsets\s3\virge\backend.c",
    "src\display16\display_component.c",
    "src\display16\loader.c",
    "src\minivdd32\minivdd_component.c",
    "tests\host\test_main.c"
)

$missing = @($required | Where-Object {
    -not (Test-Path -LiteralPath (Join-Path $repoRoot $_))
})
if ($missing.Count -ne 0) {
    throw "Required repository files are missing: $($missing -join ', ')"
}

$sourceFiles = Get-ChildItem -LiteralPath (Join-Path $repoRoot "src") -Recurse -File
$sourceFiles += Get-ChildItem -LiteralPath (Join-Path $repoRoot "include") -Recurse -File
$allowedWindowsBoundary = Join-Path $repoRoot "src\display16\loader.c"
$forbidden = $sourceFiles |
    Select-String -Pattern '#include\s*[<"]windows\.h[>"]|#include\s*[<"]vmm\.h[>"]' |
    Where-Object { $_.Path -ne $allowedWindowsBoundary }
if ($forbidden) {
    $forbidden | ForEach-Object { Write-Error $_.ToString() }
    throw "Portable skeleton source contains an unapproved Windows/DDK dependency."
}

Write-Output "Velocity9x tree check passed ($($sourceFiles.Count) source/header files)."
