[CmdletBinding()]
param([string]$BuildDirectory='build/Release', [string]$OutputDirectory='dist')
$ErrorActionPreference='Stop'
$taskRoot=Split-Path $PSScriptRoot -Parent
$taskBuild=[IO.Path]::GetFullPath((Join-Path $taskRoot $BuildDirectory))
$taskOutput=[IO.Path]::GetFullPath((Join-Path $taskRoot $OutputDirectory))
$taskName='RESONIQ-0.2.0-local-'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[Guid]::NewGuid().ToString('N').Substring(0,6)
$taskStage=Join-Path $taskOutput $taskName
New-Item -ItemType Directory -Path $taskStage,(Join-Path $taskStage 'bin'),(Join-Path $taskStage 'config'),(Join-Path $taskStage 'profiles'),(Join-Path $taskStage 'docs'),(Join-Path $taskStage 'scripts') | Out-Null
# Explicit allowlist: never recursively copy build, config or driver directories.
$taskFiles=@{
    'bin/resoniq.exe'=Join-Path $taskBuild 'resoniq.exe'
    'config/favorite.user.dps'=Join-Path $taskRoot 'config/favorite.user.dps'
    'config/session.user.dps'=Join-Path $taskRoot 'config/session.user.dps'
    'config/settings.user.txt'=Join-Path $taskRoot 'config/settings.user.txt'
    'Start-RESONIQ.cmd'=Join-Path $taskRoot 'Start-RESONIQ.cmd'
    'LICENSE'=Join-Path $taskRoot 'LICENSE'
    'THIRD_PARTY_NOTICES.md'=Join-Path $taskRoot 'THIRD_PARTY_NOTICES.md'
    'docs/STABLE_LOCAL.md'=Join-Path $taskRoot 'docs/STABLE_LOCAL.md'
    'scripts/Create-Shortcut.ps1'=Join-Path $taskRoot 'scripts/Create-Shortcut.ps1'
    'scripts/Verify-Portable.ps1'=Join-Path $taskRoot 'scripts/Verify-Portable.ps1'
}
foreach($taskProfile in @('external.dps','headphones.dps','laptop.dps','laptop-focus.dps','laptop-impact.dps','laptop-impact-plus.dps','laptop-soft.dps')) {
    $taskFiles['profiles/'+$taskProfile]=Join-Path $taskRoot ('profiles/'+$taskProfile)
}
foreach($taskEntry in $taskFiles.GetEnumerator()) {Copy-Item -LiteralPath $taskEntry.Value -Destination (Join-Path $taskStage $taskEntry.Key)}
$taskManifest=foreach($taskRelative in ($taskFiles.Keys | Sort-Object)) {
    [pscustomobject]@{path=$taskRelative;sha256=(Get-FileHash -LiteralPath (Join-Path $taskStage $taskRelative) -Algorithm SHA256).Hash}
}
ConvertTo-Json -InputObject @($taskManifest) -Depth 3 | Set-Content -LiteralPath (Join-Path $taskStage 'manifest.json') -Encoding UTF8
& (Join-Path $taskStage 'scripts/Verify-Portable.ps1')
Compress-Archive -Path $taskStage -DestinationPath ($taskStage+'.zip')
Write-Output ('Paquete: '+$taskStage+'.zip')
