[CmdletBinding()]
param(
    [string]$ControllerPath =
        "C:\everything\claude\personal\v9x-remote-agent\scripts\v9xctl.ps1",
    [string]$PackagePath,
    [string]$GuestJob = "C:\V9XREMOTE\JOBS\velocity9x-mode-matrix",
    [string]$ResultsDirectory,
    [ValidateSet("640x480x8", "800x600x8", "1024x768x8",
                 "640x480x16", "800x600x16", "1024x768x16")]
    [string[]]$Mode = @("640x480x8", "800x600x8", "1024x768x8",
                        "640x480x16", "800x600x16", "1024x768x16"),
    [ValidateRange(30, 600)]
    [int]$BootTimeoutSeconds = 180,
    [ValidateRange(1, 10)]
    [int]$Repeat = 1,
    [switch]$Json
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
if (-not $PackagePath) {
    $PackagePath = Join-Path $repoRoot "build\vm-probe\ACTIVE"
}
if (-not $ResultsDirectory) {
    $ResultsDirectory = Join-Path $repoRoot (
        "build\driver-results\mode-matrix-{0}" -f
        (Get-Date -Format "yyyyMMdd-HHmmss"))
}
foreach ($path in @($ControllerPath, $PackagePath)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Required path does not exist: $path"
    }
}
foreach ($file in @("V9XDISP.DRV", "V9XMINI.VXD", "V9XGDI.EXE",
                    "V9XPAL.EXE")) {
    if (-not (Test-Path -LiteralPath (Join-Path $PackagePath $file))) {
        throw "Mode-matrix package is missing $file."
    }
}

$powershell = Join-Path $PSHOME "powershell.exe"
$results = [IO.Path]::GetFullPath($ResultsDirectory)
New-Item -ItemType Directory -Force -Path $results | Out-Null

function Invoke-V9xCtlJson {
    param([string]$Operation, [string[]]$OperationArguments = @())
    $arguments = @(
        "-NoProfile", "-ExecutionPolicy", "Bypass", "-File",
        $ControllerPath, $Operation, "-Json"
    ) + $OperationArguments
    $lastFailure = ""
    for ($attempt = 1; $attempt -le 3; ++$attempt) {
        try {
            $lines = @(& $powershell @arguments 2>&1)
            $nativeExit = $LASTEXITCODE
        } catch {
            $lines = @($_.Exception.Message)
            $nativeExit = 1
        }
        $jsonLine = $lines | Where-Object {
            $_ -is [string] -and $_.TrimStart().StartsWith("{")
        } | Select-Object -Last 1
        if ($nativeExit -eq 0 -and $jsonLine) {
            return $jsonLine | ConvertFrom-Json
        }
        $lastFailure = $lines -join [Environment]::NewLine
        if ($attempt -lt 3) {
            Start-Sleep -Seconds 1
        }
    }
    throw "v9xctl $Operation failed after 3 attempts: $lastFailure"
}

function Invoke-GuestShell {
    param([string]$Command)
    Invoke-V9xCtlJson shell @("-Command", $Command)
}

function New-ModeRegistryFile {
    param([string]$Name, [int]$Width, [int]$Height, [int]$BitsPerPixel)
    $path = Join-Path $results ("mode-{0}.reg" -f $Name)
    $lines = @(
        "REGEDIT4", "",
        "[HKEY_LOCAL_MACHINE\System\CurrentControlSet\Services\Class\Display\0001\DEFAULT]",
        ('"Mode"="{0},{1},{2}"' -f $BitsPerPixel, $Width, $Height), "",
        "[HKEY_LOCAL_MACHINE\Config\0001\Display\Settings]",
        '"UpgradeToDefaultMode"=-',
        ('"BitsPerPixel"="{0}"' -f $BitsPerPixel),
        ('"Resolution"="{0},{1}"' -f $Width, $Height),
        '"RefreshRate"="0"'
    )
    [IO.File]::WriteAllLines($path, $lines, [Text.Encoding]::ASCII)
    $path
}

$upload = Invoke-V9xCtlJson push-tree @(
    "-Source", [IO.Path]::GetFullPath($PackagePath),
    "-Destination", $GuestJob
)
foreach ($driverFile in @("V9XDISP.DRV", "V9XMINI.VXD")) {
    $compare = Invoke-GuestShell (
        "FC /B C:\WINDOWS\SYSTEM\$driverFile $GuestJob\$driverFile")
    if ($compare.Stdout -notmatch "no differences encountered") {
        throw "Installed $driverFile does not match the package; activate it before running the matrix."
    }
}

$matrix = @()
for ($pass = 1; $pass -le $Repeat; ++$pass) {
  foreach ($name in $Mode) {
    if ($name -notmatch '^(\d+)x(\d+)x(\d+)$') {
        throw "Invalid mode name: $name"
    }
    $width = [int]$Matches[1]
    $height = [int]$Matches[2]
    $bits = [int]$Matches[3]
    $modeResults = Join-Path (Join-Path $results "pass-$pass") $name
    New-Item -ItemType Directory -Force -Path $modeResults | Out-Null
    $regFile = New-ModeRegistryFile $name $width $height $bits
    $null = Invoke-V9xCtlJson put @(
        "-Source", $regFile, "-Destination", "$GuestJob\MODE.REG")
    $null = Invoke-GuestShell "DEL C:\V9XBOOT.INI"
    $null = Invoke-GuestShell "DEL C:\V9XGDI.INI"
    $null = Invoke-GuestShell "DEL C:\V9XPAL.INI"
    $null = Invoke-GuestShell "REGEDIT /S $GuestJob\MODE.REG"

    $reboot = Invoke-V9xCtlJson reboot @(
        "-JobId", "matrix-$pass-$name",
        "-WaitSeconds", [string]$BootTimeoutSeconds)
    $desktop = Invoke-V9xCtlJson wait-desktop @(
        "-WaitSeconds", [string]$BootTimeoutSeconds)
    $info = Invoke-V9xCtlJson info
    if ($info.ScreenWidth -ne $width -or $info.ScreenHeight -ne $height -or
        $info.BitsPerPixel -ne $bits) {
        throw ("Mode {0} fell back to {1}x{2}x{3}." -f $name,
               $info.ScreenWidth, $info.ScreenHeight, $info.BitsPerPixel)
    }
    $trace = Invoke-GuestShell "TYPE C:\V9XBOOT.INI"
    if ($trace.Stdout -notmatch '(?m)^Stage=enable-ok\s*$') {
        throw "Mode $name did not reach the enable-ok driver trace."
    }

    $null = Invoke-GuestShell "START $GuestJob\V9XGDI.EXE /auto"
    $gdi = $null
    for ($attempt = 0; $attempt -lt 20; ++$attempt) {
        Start-Sleep -Milliseconds 500
        $candidate = Invoke-GuestShell (
            "IF EXIST C:\V9XGDI.INI TYPE C:\V9XGDI.INI")
        if ($candidate.Stdout -match '(?m)^Result=(PASS|FAIL)\s*$') {
            $gdi = $candidate
            break
        }
    }
    if (-not $gdi -or $gdi.Stdout -notmatch '(?m)^Result=PASS\s*$') {
        throw "Mode $name failed or timed out in the GDI framebuffer test."
    }
    foreach ($expected in @(
        "Width=$width", "Height=$height", "BitsPerPixel=$bits")) {
        if ($gdi.Stdout -notmatch "(?m)^$([regex]::Escape($expected))\s*$") {
            throw "Mode $name GDI result is missing $expected."
        }
    }
    $paletteResult = "N/A"
    if ($bits -eq 8) {
        $null = Invoke-GuestShell "START $GuestJob\V9XPAL.EXE /auto"
        $palette = $null
        for ($attempt = 0; $attempt -lt 20; ++$attempt) {
            Start-Sleep -Milliseconds 500
            $candidate = Invoke-GuestShell (
                "IF EXIST C:\V9XPAL.INI TYPE C:\V9XPAL.INI")
            if ($candidate.Stdout -match '(?m)^Result=(PASS|FAIL|SKIP)\s*$') {
                $palette = $candidate
                break
            }
        }
        if (-not $palette -or
            $palette.Stdout -notmatch '(?m)^Result=PASS\s*$') {
            throw "Mode $name failed or timed out in the palette test."
        }
        foreach ($expected in @(
            "Width=$width", "Height=$height", "BitsPerPixel=8")) {
            if ($palette.Stdout -notmatch
                "(?m)^$([regex]::Escape($expected))\s*$") {
                throw "Mode $name palette result is missing $expected."
            }
        }
        $paletteResult = "PASS"
    }
    $screenshot = Invoke-V9xCtlJson screenshot @(
        "-Destination", (Join-Path $modeResults "desktop.bmp"))
    $matrix += [pscustomobject]@{
        Pass = $pass
        Mode = $name
        BootCounter = $info.BootCounter
        DriverStage = "enable-ok"
        GdiResult = "PASS"
        PaletteResult = $paletteResult
        Screenshot = $screenshot.Destination
    }
  }
}

$summary = [pscustomobject]@{
    Success = $true
    PackagePath = [IO.Path]::GetFullPath($PackagePath)
    GuestJob = $GuestJob
    Repeat = $Repeat
    ResultsDirectory = $results
    Upload = $upload
    Matrix = $matrix
}
$summary | ConvertTo-Json -Depth 6 |
    Set-Content -LiteralPath (Join-Path $results "matrix.json") -Encoding UTF8
if ($Json) {
    $summary | ConvertTo-Json -Depth 6 -Compress
} else {
    $matrix | Format-Table -AutoSize
    Write-Output "Mode matrix passed. Results: $results"
}
