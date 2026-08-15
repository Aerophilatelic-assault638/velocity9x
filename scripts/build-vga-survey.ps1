# Build the standalone VGA hardware survey handed to owners of unsupported cards.
#
# Real-mode DOS rather than Win32, because that is the only place a single
# executable can read PCI configuration space, the video BIOS image and the raw
# VGA register file without a driver. The output folder is the whole
# distribution: the executable plus the instructions the tester needs.
[CmdletBinding()]
param([string]$BuildId)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$outputDir = Join-Path $repoRoot "build\vga-survey"
. (Join-Path $PSScriptRoot "common.ps1")
$productVersion = Get-V9xProductVersion -RepoRoot $repoRoot
if (-not $BuildId) {
    $BuildId = Get-V9xBuildId -RepoRoot $repoRoot -Fallback "vga-survey-local"
}
if ($BuildId -notmatch '^[A-Za-z0-9._+-]+$') { throw "Invalid BuildId" }

$watcomRoot = if ($env:WATCOM) { $env:WATCOM } else { "C:\WATCOM" }
$compiler = @((Join-Path $watcomRoot "binnt64\wcl.exe"),
              (Join-Path $watcomRoot "binnt\wcl.exe")) |
    Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if (-not $compiler) { throw "Open Watcom DOS compiler was not found." }
$env:WATCOM = $watcomRoot
$env:Path = "$(Join-Path $watcomRoot 'binnt64');$(Join-Path $watcomRoot 'binnt');$env:Path"
$env:INCLUDE = Join-Path $watcomRoot "h"
$source = Join-Path $repoRoot "tools\diag\vga_survey_dos.c"

# Source-level safety gate.
#
# The survey goes out to strangers running hardware nobody here can inspect, so
# the claim that it cannot alter their machine has to be enforced rather than
# reviewed. These are the only calls in the tool's vocabulary that change
# hardware state, and none of them may appear in the source at all.
$banned = @(
    @{ Pattern = '0x4f02';               Why = "VBE 4F02h set mode" },
    @{ Pattern = '0x4f14';               Why = "VBE 4F14h OEM extension" },
    @{ Pattern = '0xb10b|0xb10c|0xb10d'; Why = "PCI BIOS config-space write" },
    @{ Pattern = '0x0?cf8|0x0?cfc';      Why = "direct PCI config port access" }
)
$sourceText = (Get-Content -LiteralPath $source -Raw).ToLowerInvariant()
foreach ($rule in $banned) {
    if ($sourceText -match $rule.Pattern) {
        throw "vga_survey_dos.c contains $($rule.Why); the survey must not."
    }
}

New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
$exe = Join-Path $outputDir "V9XSURV.EXE"
Push-Location $outputDir
try {
    & $compiler "-bt=dos" "-ms" "-zq" "-wx" "-os" "-k4096" `
        "-dV9X_BUILD_ID=`"$BuildId`"" "-fe=$exe" $source
    if ($LASTEXITCODE -ne 0) { throw "Failed to build the VGA survey." }
} finally { Pop-Location }

# wcl leaves its object file in the working directory, and this directory is
# what gets handed to a tester - it holds the distribution, not build leftovers.
Get-ChildItem -LiteralPath $outputDir -Filter "*.obj" -File |
    Remove-Item -Force

$bytes = [IO.File]::ReadAllBytes($exe)
if ($bytes.Length -lt 2 -or $bytes[0] -ne 0x4d -or $bytes[1] -ne 0x5a) {
    throw "The VGA survey is not an MZ executable."
}
$text = [Text.Encoding]::ASCII.GetString($bytes)
foreach ($marker in @($BuildId, "Velocity9x VGA hardware survey", "query-only")) {
    if (-not $text.Contains($marker)) { throw "VGA survey lacks marker $marker" }
}

$readme = @"
VELOCITY9X VGA HARDWARE SURVEY
Build: $BuildId ($productVersion)

WHAT THIS IS

Velocity9x is a display driver for Windows 98. It currently supports only a
couple of S3 chips. To support your card we need to know what is actually on
it, and this program collects that: the PCI identifiers, the video BIOS, the
video modes the card advertises, your monitor's EDID, and the VGA registers.

It reads. It does not change your video mode, does not install anything, and
does not leave anything behind on the card. The only file it creates is the
report.


HOW TO RUN IT

Best results come from real DOS, not a DOS window inside Windows.

   1. In Windows 98: Start, Shut Down, "Restart in MS-DOS mode".
      (Or boot from a DOS floppy. Or if the machine only runs DOS, you are
      already there.)
   2. Change to the folder holding V9XSURV.EXE.
   3. Type:  V9XSURV
   4. Answer the one question it asks (see below).
   5. Send back the file it names at the end - normally C:\V9XSURV.INI

Running it from a DOS box inside Windows does work and still produces a
useful file. It just cannot see as much.


THE QUESTION IT ASKS

Partway through it asks whether to run the vendor-specific probe. That step
writes the unlock keys published for your chipset family, reads the registers
behind them, and puts the originals back. It is where the video memory size,
the clock settings and the aperture layout come from, so please say yes if
you can.

Saying no is fine. The main report is already saved to disk before the
question is asked, so you lose only that last section.

If Windows is running, it will recommend saying no. Take its advice.


OPTIONS

   V9XSURV /rom        also dump the complete video BIOS image. Makes the
                       report several times larger, but is the best way to
                       identify an unusual card. Please use this if asked.
   V9XSURV /tier2      say yes to the question without being asked
   V9XSURV /notier2    say no to the question without being asked
   V9XSURV /out:A:\V9XSURV.INI    write the report somewhere else, for
                       example to a floppy on a machine with no spare disk
   V9XSURV /?          show this list


WHAT IS IN THE FILE

Plain text - open it in any editor and read it before you send it. It holds
hardware identifiers and register values only: no filenames, no serial
numbers from your machine, no personal information. The one identifier that
belongs to a physical object is your monitor's EDID, which carries the
monitor's model and its factory serial number.


IF SOMETHING GOES WRONG

If the machine stops responding, power-cycle it. Nothing the survey does
survives a reboot. Then re-run with:

   V9XSURV /notier2

and send that report along with a note saying which card is fitted - the
basic report is still worth having, and knowing which card wedged the vendor
probe is itself useful.
"@

# The executable is the build's actual output. A reader holding one of these
# generated text files open - an editor, a file-transfer pane, a mounted folder
# CD - must not fail a build that has already produced and verified the binary.
$hash = (Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash
$generated = @{
    "README.TXT" = ($readme -replace "`r`n", "`n" -replace "`n", "`r`n")
    "SHA256.TXT" = "$hash  V9XSURV.EXE"
}
foreach ($name in $generated.Keys) {
    $target = Join-Path $outputDir $name
    try {
        Set-Content -LiteralPath $target -Encoding Ascii -Value $generated[$name]
    } catch [System.IO.IOException] {
        Write-Warning "Could not refresh $target; it is open in another process."
    }
}

Write-Output "Built VGA hardware survey: $exe"
Write-Output ("  {0:N0} bytes, SHA256 {1}" -f $bytes.Length, $hash)
Write-Output "  Distribution folder: $outputDir"
