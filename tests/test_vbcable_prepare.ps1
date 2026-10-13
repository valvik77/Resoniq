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
Write-Output ('PASS: official download, SHA-256, publisher/kernel signatures, tamper rejection and UI preview; no driver installed; evidence: '+$taskWork)
