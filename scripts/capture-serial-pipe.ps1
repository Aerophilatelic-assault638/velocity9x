[CmdletBinding()]
param(
    [string]$PipeName = "velocity9x-com1",
    [string]$OutputPath,
    [int]$ConnectTimeoutMilliseconds = 30000,
    [switch]$NoConsole
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
if (-not $OutputPath) {
    $OutputPath = Join-Path $repoRoot "build\vm-logs\com1-live.bin"
}

$outputDirectory = Split-Path -Parent $OutputPath
New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null

$pipe = New-Object System.IO.Pipes.NamedPipeClientStream(
    ".", $PipeName, [System.IO.Pipes.PipeDirection]::In)
$output = $null
try {
    Write-Output "Waiting for \\.\pipe\$PipeName ..."
    $pipe.Connect($ConnectTimeoutMilliseconds)
    Write-Output "Connected; capturing raw bytes to $OutputPath"

    $output = New-Object System.IO.FileStream(
        $OutputPath,
        [System.IO.FileMode]::Append,
        [System.IO.FileAccess]::Write,
        [System.IO.FileShare]::Read)
    $buffer = New-Object byte[] 4096
    while (($count = $pipe.Read($buffer, 0, $buffer.Length)) -gt 0) {
        $output.Write($buffer, 0, $count)
        $output.Flush()
        if (-not $NoConsole) {
            [Console]::Write([System.Text.Encoding]::ASCII.GetString(
                $buffer, 0, $count))
        }
    }
    Write-Output "Serial pipe disconnected."
}
finally {
    if ($output) {
        $output.Dispose()
    }
    $pipe.Dispose()
}
