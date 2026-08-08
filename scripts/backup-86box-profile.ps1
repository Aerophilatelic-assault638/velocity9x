[CmdletBinding()]
param(
    [string]$ProfilePath = "C:\Users\michael\86Box VMs\Win86SE",
    [string]$BackupRoot = "C:\Users\michael\86Box VMs\Velocity9x Backups"
)

$ErrorActionPreference = "Stop"

$profile = Get-Item -LiteralPath $ProfilePath -ErrorAction Stop
if (-not $profile.PSIsContainer) {
    throw "The VM profile path is not a directory: $ProfilePath"
}
$resolvedProfile = $profile.FullName.TrimEnd('\')
if ($resolvedProfile -ine "C:\Users\michael\86Box VMs\Win86SE") {
    throw "This recovery script is intentionally restricted to the Win86SE profile."
}

$running86Box = @(Get-Process -Name "86Box" -ErrorAction SilentlyContinue)
if ($running86Box.Count -ne 0) {
    throw "86Box is still running. Fully close the VM and manager before a cold backup."
}

$configPath = Join-Path $resolvedProfile "86box.cfg"
$configLines = Get-Content -LiteralPath $configPath -ErrorAction Stop
$diskNames = @($configLines | ForEach-Object {
    if ($_ -match '^hdd_[0-9]+_fn\s*=\s*(.+?)\s*$') {
        $matches[1]
    }
} | Where-Object { $_ } | Sort-Object -Unique)
if ($diskNames.Count -eq 0) {
    throw "The active 86box.cfg does not reference a hard-disk image."
}
$diskCandidates = @($diskNames | ForEach-Object {
    $candidatePath = [System.IO.Path]::GetFullPath((Join-Path $resolvedProfile $_))
    if (-not $candidatePath.StartsWith(
        $resolvedProfile + '\', [StringComparison]::OrdinalIgnoreCase)) {
        throw "The configured disk path leaves the VM profile: $_"
    }
    Get-Item -LiteralPath $candidatePath -ErrorAction Stop
})

$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$destination = Join-Path $BackupRoot "Win86SE-pre-velocity9x-$stamp"
if (Test-Path -LiteralPath $destination) {
    throw "Backup destination already exists: $destination"
}

New-Item -ItemType Directory -Force -Path $BackupRoot | Out-Null
New-Item -ItemType Directory -Path $destination | Out-Null
Copy-Item -LiteralPath $configPath -Destination $destination
foreach ($sourceDisk in $diskCandidates) {
    Copy-Item -LiteralPath $sourceDisk.FullName -Destination $destination
}
$nvrSource = Join-Path $resolvedProfile "nvr"
if (Test-Path -LiteralPath $nvrSource) {
    Copy-Item -LiteralPath $nvrSource -Destination $destination -Recurse
}

$manifestPath = Join-Path $destination "VELOCITY9X-BACKUP-SHA256.TXT"
$hashLines = Get-ChildItem -LiteralPath $destination -Recurse -File |
    Where-Object { $_.FullName -ne $manifestPath } |
    Sort-Object FullName |
    ForEach-Object {
        $relative = $_.FullName.Substring($destination.Length).TrimStart('\')
        $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $_.FullName).Hash
        "$hash  $relative"
    }
Set-Content -LiteralPath $manifestPath -Encoding Ascii -Value $hashLines

foreach ($sourceDisk in $diskCandidates) {
    $copiedDisk = Join-Path $destination $sourceDisk.Name
    if (-not (Test-Path -LiteralPath $copiedDisk) -or
        (Get-Item -LiteralPath $copiedDisk).Length -ne $sourceDisk.Length) {
        throw "Cold backup verification failed for $($sourceDisk.Name)."
    }
}

Write-Output "Cold VM profile backup completed: $destination"
Write-Output "Hash manifest: $manifestPath"
