$ErrorActionPreference = 'Stop'

$dst = Join-Path $env:ProgramData 'obs-studio\plugins\obs-hidden-chat-overlay'

if (-not (Test-Path $dst)) {
    Write-Host "nothing to remove at $dst"
    return
}

Write-Host "removing $dst"
Remove-Item -Recurse -Force $dst
Write-Host "done."
