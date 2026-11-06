param(
    [Parameter(Mandatory = $true)][string]$Version,
    [string]$BuildDirectory = 'out/restir-build'
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$source = Join-Path $projectRoot "$BuildDirectory/bin/Release"
$destination = Join-Path $projectRoot "out/gi-versions/$Version"
New-Item -ItemType Directory -Force $destination | Out-Null
Copy-Item -Path "$source/*" -Destination $destination -Recurse -Force
Get-ChildItem (Join-Path $projectRoot "$BuildDirectory/Tests/gi-*.ppm") -ErrorAction SilentlyContinue |
    Copy-Item -Destination $destination -Force
$commit = git -C $projectRoot rev-parse HEAD
@"
Version: $Version
Commit: $commit
Configuration: Release x64
Run: powershell -NoProfile -ExecutionPolicy Bypass -File Launch.ps1
"@ | Set-Content (Join-Path $destination 'VERSION.txt')
@'
Push-Location $PSScriptRoot
try { & (Join-Path $PSScriptRoot 'MEngineEditor.exe') @args }
finally { Pop-Location }
'@ | Set-Content (Join-Path $destination 'Launch.ps1')
Write-Host "Saved $Version ($commit) to $destination"
