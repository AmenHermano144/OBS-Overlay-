# Installs the plugin into %ProgramData%\obs-studio\plugins\.
# Run from an elevated PowerShell if ProgramData writes are blocked for you.

$ErrorActionPreference = 'Stop'

$src = Join-Path $PSScriptRoot 'obs-hidden-chat-overlay'
if (-not (Test-Path $src)) {
    Write-Error "missing source folder: $src"
}

$dstRoot = Join-Path $env:ProgramData 'obs-studio\plugins'
$dst     = Join-Path $dstRoot 'obs-hidden-chat-overlay'

Write-Host "installing to $dst"

if (Test-Path $dst) {
    Write-Host "removing previous install"
    Remove-Item -Recurse -Force $dst
}

New-Item -ItemType Directory -Force -Path $dstRoot | Out-Null
Copy-Item -Recurse -Force $src $dstRoot

Write-Host "done. launch OBS and check Help > Log Files > View Current Log for"
Write-Host "    [obs-hidden-chat-overlay] loaded"
