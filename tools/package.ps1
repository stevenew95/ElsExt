# Builds release\ElsExt-<version>.zip from a Release build.
# Usage: powershell -File tools\package.ps1 -Version 1.0.0
param([Parameter(Mandatory = $true)][string]$Version)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$asi = Join-Path $root 'bin\ElsExt.asi'
if (-not (Test-Path $asi)) { throw "Build ElsExt first: $asi not found." }

$stage = Join-Path $root "release\ElsExt-$Version"
$zip = Join-Path $root "release\ElsExt-$Version.zip"
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
if (Test-Path $zip) { Remove-Item $zip -Force }
New-Item -ItemType Directory -Force (Join-Path $stage 'examples') | Out-Null

Copy-Item $asi $stage
Copy-Item (Join-Path $root 'ElsExt.ini') $stage
Copy-Item (Join-Path $root 'package\Installation and Setup.txt') $stage
Copy-Item (Join-Path $root 'examples\example.xml') (Join-Path $stage 'examples')
Copy-Item (Join-Path $root 'LICENSE') (Join-Path $stage 'LICENSE.txt')
Copy-Item (Join-Path $root 'THIRD_PARTY_NOTICES.txt') $stage

Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip
Write-Output "Created $zip"
