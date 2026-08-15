# Read V9XSURV.INI reports sent back by testers and turn them into something
# actionable.
#
# The DOS tool deliberately captures rather than interprets, so all the decoding
# lives here: EDID, PCI BARs, the ROM image. That split means a decoding mistake
# is fixed by editing this file and re-running it over every report already
# collected, instead of shipping a new executable to everyone who helped.
[CmdletBinding()]
param(
    # One report, or a folder of them.
    [Parameter(Mandatory = $true, Position = 0)]
    [string]$Path,
    # Append a normalised one-line-per-card record here.
    [string]$Csv,
    # Reconstruct the captured video BIOS image to this file (single report only).
    [string]$ExtractRom,
    # Reconstruct the captured EDID block (single report only).
    [string]$ExtractEdid,
    [switch]$Detailed
)

$ErrorActionPreference = "Stop"

function Read-IniFile {
    param([string]$LiteralPath)

    $data = [ordered]@{}
    $section = "(root)"
    $data[$section] = [ordered]@{}
    foreach ($line in [IO.File]::ReadAllLines($LiteralPath)) {
        $trimmed = $line.Trim()
        if ($trimmed.Length -eq 0 -or $trimmed.StartsWith(";")) { continue }
        if ($trimmed.StartsWith("[") -and $trimmed.EndsWith("]")) {
            $section = $trimmed.Substring(1, $trimmed.Length - 2)
            if (-not $data.Contains($section)) { $data[$section] = [ordered]@{} }
            continue
        }
        # Split on the first '=' only: extracted BIOS strings can contain more.
        $split = $trimmed.IndexOf("=")
        if ($split -lt 1) { continue }
        $key = $trimmed.Substring(0, $split)
        $value = $trimmed.Substring($split + 1)
        $data[$section][$key] = $value
    }
    return $data
}

function Get-IniValue {
    param($Ini, [string]$Section, [string]$Key, $Default = $null)
    if (-not $Ini.Contains($Section)) { return $Default }
    if (-not $Ini[$Section].Contains($Key)) { return $Default }
    return $Ini[$Section][$Key]
}

# Blob sections are offset-keyed hex lines. One rule reassembles config space,
# the ROM image and EDID alike.
function Get-Blob {
    param($Ini, [string]$Section, [string]$Prefix)

    if (-not $Ini.Contains($Section)) { return $null }
    $chunks = @{}
    foreach ($key in $Ini[$Section].Keys) {
        if ($key -notlike "$Prefix.*") { continue }
        $offsetText = $key.Substring($Prefix.Length + 1)
        $offset = [Convert]::ToInt32($offsetText, 16)
        $chunks[$offset] = $Ini[$Section][$key]
    }
    if ($chunks.Count -eq 0) { return $null }

    $bytes = New-Object System.Collections.Generic.List[byte]
    foreach ($offset in ($chunks.Keys | Sort-Object)) {
        $hex = $chunks[$offset] -replace '[^0-9A-Fa-f]', ''
        if ($bytes.Count -ne $offset) {
            Write-Warning "$Section/$Prefix has a gap or overlap at offset $offset"
        }
        for ($i = 0; $i + 1 -lt $hex.Length; $i += 2) {
            $bytes.Add([Convert]::ToByte($hex.Substring($i, 2), 16))
        }
    }
    return $bytes.ToArray()
}

function ConvertFrom-Edid {
    param([byte[]]$Bytes)

    if ($null -eq $Bytes -or $Bytes.Length -lt 128) { return $null }
    $header = @(0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00)
    for ($i = 0; $i -lt 8; $i++) {
        if ($Bytes[$i] -ne $header[$i]) { return [pscustomobject]@{ Valid = $false } }
    }
    $sum = 0
    for ($i = 0; $i -lt 128; $i++) { $sum = ($sum + $Bytes[$i]) -band 0xFF }

    # Every byte is widened to [int] before shifting. Windows PowerShell 5.1
    # keeps a shift inside the width of its left operand, so [byte]0x4D -shl 8
    # is 0, not 0x4D00 - which silently corrupts every multi-byte field.
    # Bytes 8-9 pack three letters into five bits each, 1 = 'A'.
    $packed = ([int]$Bytes[8] -shl 8) -bor [int]$Bytes[9]
    $vendor = ""
    foreach ($shift in 10, 5, 0) {
        $vendor += [char]((($packed -shr $shift) -band 0x1F) + 64)
    }

    # The first detailed timing descriptor is the monitor's preferred mode.
    $d = 54
    $preferred = $null
    if ($Bytes[$d] -ne 0 -or $Bytes[$d + 1] -ne 0) {
        $clock = ((([int]$Bytes[$d + 1] -shl 8) -bor [int]$Bytes[$d])) * 10
        $hActive = [int]$Bytes[$d + 2] -bor
                   (((([int]$Bytes[$d + 4] -shr 4) -band 0x0F)) -shl 8)
        $vActive = [int]$Bytes[$d + 5] -bor
                   (((([int]$Bytes[$d + 7] -shr 4) -band 0x0F)) -shl 8)
        $preferred = "{0}x{1} @ {2:N2} MHz" -f $hActive, $vActive, ($clock / 1000)
    }

    # Descriptors tagged FCh carry the monitor's model name.
    $name = ""
    foreach ($base in 54, 72, 90, 108) {
        if ($Bytes[$base] -eq 0 -and $Bytes[$base + 1] -eq 0 -and
            $Bytes[$base + 3] -eq 0xFC) {
            $raw = [Text.Encoding]::ASCII.GetString($Bytes, $base + 5, 13)
            $name = ($raw -split "`n")[0].Trim()
        }
    }

    return [pscustomobject]@{
        Valid        = $true
        ChecksumOk   = ($sum -eq 0)
        Manufacturer = $vendor
        ProductCode  = "{0:X4}" -f (([int]$Bytes[11] -shl 8) -bor [int]$Bytes[10])
        Serial       = "{0:X8}" -f ([BitConverter]::ToUInt32($Bytes, 12))
        Week         = $Bytes[16]
        Year         = 1990 + $Bytes[17]
        Version      = "$($Bytes[18]).$($Bytes[19])"
        MonitorName  = $name
        Preferred    = $preferred
        Extensions   = $Bytes[126]
    }
}

function ConvertFrom-Bar {
    param([string]$Raw)

    if ([string]::IsNullOrWhiteSpace($Raw)) { return $null }
    # [int64] throughout: a 32-bit BAR with the top bit set is negative as an
    # Int32, which turns the masks below into nonsense.
    $value = [int64][Convert]::ToUInt32($Raw, 16)
    if ($value -eq 0) { return $null }
    if (($value -band 1) -ne 0) {
        return [pscustomobject]@{
            Space = "io"; Base = "{0:X8}" -f ($value -band 0xFFFFFFFCL)
            Prefetchable = $false; Type = "32"
        }
    }
    $type = switch (($value -shr 1) -band 0x3) {
        0 { "32" } 2 { "64" } default { "reserved" }
    }
    return [pscustomobject]@{
        Space        = "memory"
        Base         = "{0:X8}" -f ($value -band 0xFFFFFFF0L)
        Prefetchable = ((($value -shr 3) -band 1) -ne 0)
        Type         = $type
    }
}

function Read-SurveyReport {
    param([string]$LiteralPath)

    $ini = Read-IniFile -LiteralPath $LiteralPath
    $problems = @()

    if ((Get-IniValue $ini "Report" "Tool") -ne "V9XSURV") {
        throw "$LiteralPath is not a Velocity9x VGA survey report."
    }
    $schema = Get-IniValue $ini "Report" "SchemaVersion"
    if ($schema -ne "1") { $problems += "unknown schema version '$schema'" }
    # Complete is the last key the tool writes. Without it the file was cut off
    # in transit and nothing in it should be trusted wholesale.
    if ((Get-IniValue $ini "Result" "Complete") -ne "yes") {
        $problems += "truncated report (no Result/Complete marker)"
    }
    if ((Get-IniValue $ini "VGARegisters" "Trust") -eq "virtualized") {
        $problems += "VGA registers captured under Windows; not hardware values"
    }
    if ((Get-IniValue $ini "Tier2" "Requested") -ne "yes") {
        $problems += "vendor probe declined; no chipset register detail"
    }

    # The first display-class device is the card of interest.
    $device = $null
    if ($ini.Contains("PciDevice.0")) { $device = $ini["PciDevice.0"] }

    $edid = ConvertFrom-Edid (Get-Blob $ini "EDID" "Block0")

    $vendorId = if ($device) { $device["VendorId"] } else { "" }
    $deviceId = if ($device) { $device["DeviceId"] } else { "" }

    $bars = @()
    if ($device) {
        foreach ($n in 0..5) {
            $decoded = ConvertFrom-Bar $device["Bar$n"]
            if ($decoded) {
                $bars += "BAR${n}:$($decoded.Space)@$($decoded.Base)" +
                         $(if ($decoded.Prefetchable) { "(pf)" } else { "" })
            }
        }
    }

    $romStrings = @()
    if ($ini.Contains("VideoBios")) {
        foreach ($key in $ini["VideoBios"].Keys) {
            if ($key -like "String.*") { $romStrings += $ini["VideoBios"][$key] }
        }
    }

    return [pscustomobject]@{
        File         = Split-Path -Leaf $LiteralPath
        FullPath     = $LiteralPath
        Ini          = $ini
        Build        = Get-IniValue $ini "Report" "Build"
        Date         = Get-IniValue $ini "Report" "Date"
        Note         = Get-IniValue $ini "Report" "Note"
        Windows      = Get-IniValue $ini "System" "WindowsPresent"
        VendorId     = $vendorId
        DeviceId     = $deviceId
        SubsystemId  = if ($device) {
            "$($device['SubsystemVendorId']):$($device['SubsystemId'])"
        } else { "" }
        Revision     = if ($device) { $device["Revision"] } else { "" }
        ClassCode    = if ($device) { $device["ClassCode"] } else { "" }
        Bars         = ($bars -join " ")
        DisplayCount = Get-IniValue $ini "Result" "DisplayDeviceCount" "0"
        VbeVersion   = Get-IniValue $ini "VBE" "Version"
        VideoMemory  = Get-IniValue $ini "VBE" "TotalMemoryBytes"
        OemString    = Get-IniValue $ini "VBE" "OemString"
        ModeCount    = Get-IniValue $ini "VBEModes" "Count" "0"
        RomStrings   = $romStrings
        Chipset      = @($ini.Keys | Where-Object { $_ -like "Chipset*" }) -join ","
        Monitor      = if ($edid -and $edid.Valid) {
            "$($edid.Manufacturer) $($edid.MonitorName)".Trim()
        } else { "" }
        Edid         = $edid
        Problems     = $problems
    }
}

# ---------------------------------------------------------------------------

$targets = @()
if (Test-Path -LiteralPath $Path -PathType Container) {
    $targets = @(Get-ChildItem -LiteralPath $Path -Filter "*.ini" -Recurse -File |
        ForEach-Object { $_.FullName })
} else {
    $targets = @((Resolve-Path -LiteralPath $Path).Path)
}
if ($targets.Count -eq 0) { throw "No report files found under $Path." }

$reports = @()
foreach ($target in $targets) {
    try {
        $reports += Read-SurveyReport -LiteralPath $target
    } catch {
        Write-Warning "Skipped $target : $($_.Exception.Message)"
    }
}
if ($reports.Count -eq 0) { throw "No readable survey reports." }

if ($ExtractRom) {
    if ($reports.Count -ne 1) { throw "-ExtractRom needs exactly one report." }
    $rom = Get-Blob $reports[0].Ini "VideoBios" "Rom"
    if ($null -eq $rom) { throw "That report carries no ROM image." }
    [IO.File]::WriteAllBytes($ExtractRom, $rom)
    $scope = Get-IniValue $reports[0].Ini "VideoBios" "DumpScope"
    Write-Output ("Wrote {0:N0} bytes to {1} (scope: {2})" -f $rom.Length, $ExtractRom, $scope)
    if ($scope -ne "full-image") {
        Write-Warning "This is a partial image. Ask the tester to re-run with /rom."
    }
}

if ($ExtractEdid) {
    if ($reports.Count -ne 1) { throw "-ExtractEdid needs exactly one report." }
    $block = Get-Blob $reports[0].Ini "EDID" "Block0"
    if ($null -eq $block) { throw "That report carries no EDID block." }
    [IO.File]::WriteAllBytes($ExtractEdid, $block)
    Write-Output ("Wrote {0:N0} bytes to {1}" -f $block.Length, $ExtractEdid)
}

foreach ($report in $reports) {
    Write-Output ""
    Write-Output ("=== {0} ===" -f $report.File)
    Write-Output ("  Card         {0}:{1} rev {2}  class {3}" -f
        $report.VendorId, $report.DeviceId, $report.Revision, $report.ClassCode)
    Write-Output ("  Subsystem    {0}" -f $report.SubsystemId)
    Write-Output ("  Apertures    {0}" -f $report.Bars)
    Write-Output ("  VBE          v{0}, {1:N0} bytes VRAM, {2} modes" -f
        $report.VbeVersion, [int64]$report.VideoMemory, $report.ModeCount)
    Write-Output ("  OEM string   {0}" -f $report.OemString)
    if ($report.Monitor) { Write-Output ("  Monitor      {0}" -f $report.Monitor) }
    if ($report.Note)    { Write-Output ("  Tester note  {0}" -f $report.Note) }
    if ($report.RomStrings.Count -gt 0) {
        Write-Output "  ROM strings:"
        foreach ($s in ($report.RomStrings | Select-Object -First 6)) {
            Write-Output "      $s"
        }
    }
    foreach ($problem in $report.Problems) { Write-Warning "  $problem" }

    if ($Detailed -and $report.Edid -and $report.Edid.Valid) {
        Write-Output "  EDID:"
        $report.Edid | Format-List | Out-String | Write-Output
    }
}

if ($Csv) {
    $rows = $reports | Select-Object File, Date, Build, VendorId, DeviceId,
        Revision, SubsystemId, ClassCode, VideoMemory, VbeVersion, ModeCount,
        OemString, Monitor, Windows, Note,
        @{ n = "Problems"; e = { $_.Problems -join "; " } }
    if (Test-Path -LiteralPath $Csv) {
        $rows | Export-Csv -LiteralPath $Csv -NoTypeInformation -Append
    } else {
        $rows | Export-Csv -LiteralPath $Csv -NoTypeInformation
    }
    Write-Output ""
    Write-Output "Appended $($rows.Count) row(s) to $Csv"
}
