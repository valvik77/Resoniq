$ErrorActionPreference='Stop'
$taskRoot=Split-Path $PSScriptRoot -Parent
$taskScript=Join-Path $taskRoot 'installer/Prepare-VBCABLE.ps1'
$taskWork=Join-Path $taskRoot ('build/vbcable-prepare-test-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $taskWork | Out-Null
$taskPowerShell="$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe"
# Exercise the actual download path, without launching/installing a driver.
$taskResult=& $taskPowerShell -NoProfile -ExecutionPolicy Bypass -File $taskScript -VerifyOnly -DataDirectory $taskWork
if($LASTEXITCODE -ne 0){throw 'Download/signature verification failed'}
$taskResult=$taskResult | ConvertFrom-Json
if(-not $taskResult.verified -or $taskResult.installed){throw 'Verification mode installs or fails verification'}
$taskAltered=Join-Path $taskWork 'altered.zip'
[IO.File]::WriteAllBytes($taskAltered,[IO.File]::ReadAllBytes($taskResult.package))
$taskBytes=[IO.File]::ReadAllBytes($taskAltered);$taskBytes[100]=$taskBytes[100] -bxor 1;[IO.File]::WriteAllBytes($taskAltered,$taskBytes)
$taskRejectedOutput=& $taskPowerShell -NoProfile -ExecutionPolicy Bypass -File $taskScript -VerifyOnly -PackagePath $taskAltered -DataDirectory $taskWork 2>&1
if($LASTEXITCODE -eq 0){throw 'Altered prerequisite accepted'}
$taskPreview=Start-Process -FilePath $taskPowerShell -ArgumentList @('-NoProfile','-ExecutionPolicy','Bypass','-File',('"'+$taskScript+'"'),'-PreviewOnly','-DataDirectory',('"'+$taskWork+'"')) -WindowStyle Hidden -PassThru
if(-not $taskPreview.WaitForExit(10000)){throw 'Assistant preview timeout'}
if($taskPreview.ExitCode -ne 0 -or -not (Test-Path -LiteralPath (Join-Path $taskWork 'assistant.png'))){throw 'Assistant UI failed'}
$taskPreparePreview=Start-Process -FilePath $taskPowerShell -ArgumentList @('-NoProfile','-ExecutionPolicy','Bypass','-File',('"'+$taskScript+'"'),'-PreparePreview','-PackagePath',('"'+$taskResult.package+'"'),'-DataDirectory',('"'+$taskWork+'"')) -WindowStyle Hidden -PassThru
if(-not $taskPreparePreview.WaitForExit(20000)){throw 'Async assistant preparation timeout'}
if($taskPreparePreview.ExitCode -ne 0 -or -not (Test-Path -LiteralPath (Join-Path $taskWork 'prepare-result.json'))){throw 'Async assistant preparation failed'}
# Detecting an installed cable: run the assistant's own function in the same
# Windows PowerShell it ships with, against a stand-in that returns exactly one
# CIM device. A lone CimInstance has no Count there, which hid installed cables.
$taskSource=Get-Content -LiteralPath $taskScript -Raw
if($taskSource -match '(?<!@)\(Find-CableDevice\)\.Count'){throw 'Installed cable check counts an unrolled result'}
$taskFunction=[regex]::Match($taskSource,'(?s)function Find-CableDevice \{.*?\n\}').Value
if(-not $taskFunction){throw 'Cable detection function not found'}
$taskCondition=[regex]::Match($taskSource,'@\(Find-CableDevice\)\.Count -gt 0').Value
if(-not $taskCondition){throw 'Installed cable condition not found'}
$taskProbe=Join-Path $taskWork 'detect-one.ps1'
@(
 $taskFunction.Replace('Get-CimInstance Win32_PnPEntity','Get-CimInstance Win32_OperatingSystem | Add-Member -NotePropertyName HardwareID -NotePropertyValue @(''VBAudioVACWDM'') -PassThru -Force')
 ('if('+$taskCondition+'){exit 0}else{exit 3}')
) | Set-Content -LiteralPath $taskProbe -Encoding ASCII
$taskDetect=Start-Process -FilePath $taskPowerShell -ArgumentList @('-NoProfile','-ExecutionPolicy','Bypass','-File',('"'+$taskProbe+'"')) -WindowStyle Hidden -PassThru
if(-not $taskDetect.WaitForExit(20000)){throw 'Installed cable detection timeout'}
if($taskDetect.ExitCode -ne 0){throw 'A single installed cable is not detected in Windows PowerShell'}
Write-Output ('PASS: official download, SHA-256, publisher/kernel signatures, tamper rejection, UI preview and single installed cable detection; no driver installed; evidence: '+$taskWork)
