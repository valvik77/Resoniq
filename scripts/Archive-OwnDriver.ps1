[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
$taskRoot=Split-Path $PSScriptRoot -Parent
$taskArchive=Join-Path $taskRoot ('archives/own-driver-'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[Guid]::NewGuid().ToString('N').Substring(0,6))
New-Item -ItemType Directory -Path $taskArchive | Out-Null
$taskFiles=@('CMakeLists.txt','LICENSE','THIRD_PARTY_NOTICES.md','AGENTS.md','README.md','scripts/Archive-OwnDriver.ps1')
foreach($taskDirectory in @('apps','include','src','tests','installer','profiles')) {
    foreach($taskFile in (Get-ChildItem -LiteralPath (Join-Path $taskRoot $taskDirectory) -Recurse -File)) {
        if($taskFile.Extension -in @('.cpp','.hpp','.h','.ps1','.py','.wxs','.dps')) {$taskFiles+=[IO.Path]::GetRelativePath($taskRoot,$taskFile.FullName)}
    }
}
foreach($taskFile in (Get-ChildItem -LiteralPath (Join-Path $taskRoot 'drivers/virtual') -File)) {
    if($taskFile.Extension -in @('.py','.ps1','.md','.inf','.rc','.config')){$taskFiles+=[IO.Path]::GetRelativePath($taskRoot,$taskFile.FullName)}
}
foreach($taskFile in (Get-ChildItem -LiteralPath (Join-Path $taskRoot 'drivers/virtual/overlay') -Recurse -File)) {$taskFiles+=[IO.Path]::GetRelativePath($taskRoot,$taskFile.FullName)}
foreach($taskName in @('DpsAudioVirtual.sys','DpsAudioVirtual.inf','DpsAudioVirtual.cat','DpsAudioVirtual.pdb','DpsAudioDevelopment.cer','MS-PL.txt','build-info.json')) {$taskFiles+='drivers/virtual/build/'+$taskName}
foreach($taskName in @('PROJECT_MEMORY.md','CODEX_HANDOFF.md','virtual-device.md','architecture.md','INSTALLATION.md','OWN_DRIVER_RECOVERY.md')){$taskFiles+='docs/'+$taskName}
$taskFiles+=@('dist/RESONIQ-Setup-0.2.0-x64-Development.msi','dist/RESONIQ-Driver-Submission-Unsigned.cab','build/installer-release/Release/resoniq.exe','build/installer-release/Release/resoniq_device.exe')
$taskManifest=foreach($taskRelative in ($taskFiles | Sort-Object -Unique)) {
    if($taskRelative -match '(?i)\.pfx$|\.key$|\.wav$|\.user\.dps$'){throw 'Archivo privado no permitido en la instantanea'}
    $taskSource=Join-Path $taskRoot $taskRelative
    $taskDestination=Join-Path $taskArchive $taskRelative
    New-Item -ItemType Directory -Path (Split-Path $taskDestination -Parent) -Force | Out-Null
    Copy-Item -LiteralPath $taskSource -Destination $taskDestination
    [pscustomobject]@{path=$taskRelative;sha256=(Get-FileHash -LiteralPath $taskDestination).Hash}
}
ConvertTo-Json -InputObject @($taskManifest) -Depth 3 | Set-Content -LiteralPath (Join-Path $taskArchive 'manifest.json') -Encoding UTF8
Compress-Archive -Path $taskArchive -DestinationPath ($taskArchive+'.zip')
foreach($taskEntry in $taskManifest){if((Get-FileHash -LiteralPath (Join-Path $taskArchive $taskEntry.path)).Hash -ne $taskEntry.sha256){throw 'Error al verificar instantanea'}}
Write-Output ('PASS: '+$taskManifest.Count+' archivos conservados; '+$taskArchive+'.zip')
