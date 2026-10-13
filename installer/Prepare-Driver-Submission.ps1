[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
$taskRoot=Split-Path $PSScriptRoot -Parent
$taskDriver=Join-Path $taskRoot 'drivers/virtual/build'
$taskDirectory=Join-Path $taskRoot 'build/driver-submission'
$taskOutput=Join-Path $taskRoot 'dist'
New-Item -ItemType Directory -Path $taskDirectory,$taskOutput -Force | Out-Null
$taskDdf=@('.OPTION EXPLICIT','.Set Cabinet=on','.Set Compress=on','.Set CompressionType=LZX','.Set MaxDiskSize=0','.Set CabinetNameTemplate=RESONIQ-Driver-Submission-Unsigned.cab',('.Set DiskDirectory1="'+$taskOutput+'"'))
foreach($taskFile in @('DpsAudioVirtual.sys','DpsAudioVirtual.inf','DpsAudioVirtual.cat','DpsAudioVirtual.pdb')) {
    $taskSource=Join-Path $taskDriver $taskFile
    if(-not (Test-Path -LiteralPath $taskSource)){throw ('Falta archivo de envio: '+$taskFile)}
    $taskDdf+=('"'+$taskSource+'" "DpsAudioVirtual\'+$taskFile+'"')
}
$taskDdfPath=Join-Path $taskDirectory 'submission.ddf'
$taskDdf | Set-Content -LiteralPath $taskDdfPath -Encoding ASCII
Push-Location $taskDirectory
try {$taskCabOutput=@(& "$env:SystemRoot\System32\makecab.exe" /f $taskDdfPath 2>&1); if($LASTEXITCODE -ne 0){throw ($taskCabOutput -join "`n")}}finally{Pop-Location}
Write-Output 'CAB preparado con SYS/INF/CAT/PDB. Aun debe firmarse con el certificado asociado a Partner Center antes del envio. No es un controlador de produccion.'
