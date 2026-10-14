[CmdletBinding()]
param([string]$MsiPath='dist/RESONIQ-Setup-0.2.0-x64-Development.msi',[switch]$Cable,[switch]$SkipAudioTest)
$ErrorActionPreference='Stop'
$taskRoot=Split-Path $PSScriptRoot -Parent
$taskMsi=[IO.Path]::GetFullPath((Join-Path $taskRoot $MsiPath))
$taskWork=Join-Path $taskRoot ('build/installer-test-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $taskWork | Out-Null
$taskWix=Join-Path $taskRoot 'build/tools/wix/wix.exe'
& $taskWix msi decompile $taskMsi -o (Join-Path $taskWork 'package.wxs') -x (Join-Path $taskWork 'extracted')
if($LASTEXITCODE -ne 0){throw 'MSI extraction failed'}
[xml]$taskXml=Get-Content -LiteralPath (Join-Path $taskWork 'package.wxs') -Raw
$taskManager=[Xml.XmlNamespaceManager]::new($taskXml.NameTable);$taskManager.AddNamespace('w','http://wixtoolset.org/schemas/v4/wxs')
if($Cable) {
    $taskVolume=$taskXml.SelectSingleNode('//w:Component[@Id="CableWindowsVolume"]',$taskManager)
    $taskValue=$taskVolume.SelectSingleNode('w:RegistryValue',$taskManager)
    if($taskVolume.Bitness -ne 'always64' -or $taskVolume.Permanent -ne 'yes' -or $taskValue.Root -ne 'HKLM' -or $taskValue.Key -ne 'SOFTWARE\VB-Audio\Cable' -or $taskValue.Name -ne 'VBAudioCableWDM_VolCtl' -or $taskValue.Type -ne 'integer' -or $taskValue.Value -ne '1'){throw 'VB Windows volume configuration missing or invalid'}
    if(-not $taskXml.SelectSingleNode('//w:Feature[@Id="Application"]/w:ComponentRef[@Id="CableWindowsVolume"]',$taskManager)){throw 'VB volume configuration is not installed'}
    foreach($taskAction in $taskXml.SelectNodes('//w:CustomAction',$taskManager)) {
        if($taskAction.Id -notin @('LaunchInstructions','LaunchApplication','CloseRunningResoniq','SetWIXUI_EXITDIALOGOPTIONALCHECKBOX')){throw 'Cable MSI runs custom driver actions'}
    }
    # An upgrade must not offer the VB-CABLE installer by default: it only clears the checkbox property, in the UI sequence.
    $taskUncheck=$taskXml.SelectSingleNode('//w:CustomAction[@Id="SetWIXUI_EXITDIALOGOPTIONALCHECKBOX"]',$taskManager)
    if($taskUncheck.Property -ne 'WIXUI_EXITDIALOGOPTIONALCHECKBOX' -or $taskUncheck.Value -ne '{}' -or $taskUncheck.DllEntry -or $taskUncheck.ExeCommand){throw 'Upgrade checkbox action is not a plain property reset'}
    $taskUncheckSequence=$taskXml.SelectSingleNode('//w:InstallUISequence/w:Custom[@Action="SetWIXUI_EXITDIALOGOPTIONALCHECKBOX"]',$taskManager)
    if($taskUncheckSequence.Condition -ne 'WIX_UPGRADE_DETECTED' -or $taskXml.SelectSingleNode('//w:InstallExecuteSequence/w:Custom[@Action="SetWIXUI_EXITDIALOGOPTIONALCHECKBOX"]',$taskManager)){throw 'Upgrade checkbox reset is missing, unconditional or outside the UI sequence'}
    if($taskXml.SelectSingleNode('//w:InstallExecuteSequence/w:Custom[@Action="LaunchInstructions"]',$taskManager)){throw 'Instructions run as an install transaction action'}
    if($taskXml.SelectSingleNode('//w:InstallExecuteSequence/w:Custom[@Action="LaunchApplication"]',$taskManager)){throw 'Application starts inside the install transaction'}
    $taskCloseAction=$taskXml.SelectSingleNode('//w:CustomAction[@Id="CloseRunningResoniq"]',$taskManager)
    if($taskCloseAction.DllEntry -ne 'CloseResoniq' -or $taskCloseAction.Return -eq 'ignore'){throw 'Graceful close action missing or ignores failures'}
    $taskCloseSequence=$taskXml.SelectSingleNode('//w:InstallExecuteSequence/w:Custom[@Action="CloseRunningResoniq"]',$taskManager)
    if(-not $taskCloseSequence -or $taskCloseSequence.Condition -notmatch 'REMOVE'){throw 'Graceful close sequence missing'}
    $taskAppAction=$taskXml.SelectSingleNode('//w:CustomAction[@Id="LaunchApplication"]',$taskManager)
    if($taskAppAction.DllEntry -ne 'WixUnelevatedShellExec'){throw 'Application launch is not unelevated'}
    if($taskXml.SelectSingleNode('//w:Property[@Id="WixUnelevatedShellExecTarget"]',$taskManager).Value -ne '[#ResoniqExecutable]'){throw 'Application launch target missing'}
    if(-not $taskXml.SelectSingleNode('//w:File[@Id="ResoniqExecutable"]',$taskManager)){throw 'Application launch file missing'}
    $taskAppPublish=$taskXml.SelectSingleNode('//w:Dialog[@Id="ExitDialog"]/w:Control[@Id="Finish"]/w:Publish[@Event="DoAction"][@Value="LaunchApplication"]',$taskManager)
    if(-not $taskAppPublish -or $taskAppPublish.Condition -notmatch 'REMOVE' -or $taskAppPublish.Condition -match 'NOT Installed|WIXUI_EXITDIALOGOPTIONALCHECKBOX'){throw 'Application does not start automatically after install/upgrade'}
    if(-not $taskXml.SelectSingleNode('//w:Property[@Id="WIXUI_EXITDIALOGOPTIONALCHECKBOXTEXT"]',$taskManager)){throw 'Installer offers no prerequisite instructions'}
    foreach($taskShortcut in @('CableWebsite','CableInstaller','Instructions')) {
        if(-not $taskXml.SelectSingleNode('//w:Shortcut[@Id="'+$taskShortcut+'"]',$taskManager)){throw 'Cable prerequisites shortcut missing'}
    }
} else {
$taskFeature=$taskXml.SelectSingleNode('//w:Feature[@Id="VirtualDevice"]',$taskManager)
if($taskFeature.Level -ne '0'){throw 'Development MSI exposes automatic driver installation'}
$taskMode=$taskXml.SelectSingleNode('//w:Property[@Id="DpsSigningMode"]',$taskManager)
$taskInstallSequence=$taskXml.SelectSingleNode('//w:InstallExecuteSequence/w:Custom[@Action="InstallDevice"]',$taskManager)
if($taskMode.Value -ne 'development' -or $taskInstallSequence.Condition -notmatch 'DpsSigningMode="production"'){throw 'Development MSI can execute driver installation'}
$taskInstall=$taskXml.SelectSingleNode('//w:CustomAction[@Id="InstallDevice"]',$taskManager)
if($taskInstall.Execute -ne 'deferred' -or $taskInstall.Impersonate -ne 'no'){throw 'Driver action is not elevated/deferred'}
$taskRemove=$taskXml.SelectSingleNode('//w:InstallExecuteSequence/w:Custom[@Action="RemoveDeviceOwned"]',$taskManager)
if($taskRemove.Condition -notmatch 'NOT UPGRADINGPRODUCTCODE'){throw 'Upgrade removes the existing device'}
$taskRollback=$taskXml.SelectSingleNode('//w:CustomAction[@Id="RollbackDevice"]',$taskManager)
if($taskRollback.Execute -ne 'rollback' -or $taskRollback.ExeCommand -notmatch '-Rollback'){throw 'Driver rollback missing'}
}
$taskLaunch=$taskXml.SelectSingleNode('//w:Launch',$taskManager)
if($taskLaunch.Condition -notmatch 'Msix64' -or $taskLaunch.Condition -notmatch '22621'){throw 'Architecture/OS checks missing'}
if($taskLaunch.Condition -notmatch '^Installed OR '){throw 'OS check blocks maintenance'}
$taskSecure=$taskXml.SelectSingleNode('//w:Property[@Id="WINDOWSBUILD"]',$taskManager)
if($taskSecure.Secure -ne 'yes'){throw 'Windows build search is lost during elevation'}
# Evaluate the actual MSI condition without running an installation transaction.
$taskEngine=New-Object -ComObject WindowsInstaller.Installer
if($Cable) {
    $taskDb=$taskEngine.OpenDatabase($taskMsi,0)
    $taskView=$taskDb.OpenView('SELECT `Sequence` FROM `InstallExecuteSequence` WHERE `Action`=''CloseRunningResoniq''')
    $taskView.Execute();$taskCloseRow=$taskView.Fetch();$taskCloseOrder=$taskCloseRow.IntegerData(1)
    $taskView.Close();[void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($taskCloseRow);[void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($taskView)
    $taskView=$taskDb.OpenView('SELECT `Sequence` FROM `InstallExecuteSequence` WHERE `Action`=''InstallValidate''')
    $taskView.Execute();$taskValidateRow=$taskView.Fetch();$taskValidateOrder=$taskValidateRow.IntegerData(1)
    $taskView.Close();[void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($taskValidateRow);[void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($taskView)
    [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($taskDb)
    if($taskCloseOrder -ge $taskValidateOrder){throw 'Application closes after MSI file-in-use validation'}
}
$taskSession=$taskEngine.OpenPackage($taskMsi,1)
$taskCondition=$taskLaunch.Condition
if($Cable) {
    $taskAppCondition=$taskAppPublish.Condition
    foreach($taskInstalledState in @('','1')) {
        $taskSession.Property('Installed')=$taskInstalledState;$taskSession.Property('REMOVE')=''
        if($taskSession.EvaluateCondition($taskAppCondition) -ne 1){throw 'Application will not launch after install/maintenance'}
    }
    $taskSession.Property('REMOVE')='ALL'
    if($taskSession.EvaluateCondition($taskAppCondition) -ne 0){throw 'Uninstall launches application'}
    $taskSession.Property('REMOVE')=''
}
$taskSession.Property('Installed')='1';$taskSession.Property('WINDOWSBUILD')=''
if($taskSession.EvaluateCondition($taskCondition) -ne 1){throw 'Maintenance fails with missing OS search'}
$taskSession.Property('Installed')='';$taskSession.Property('WINDOWSBUILD')='19045'
if($taskSession.EvaluateCondition($taskCondition) -ne 0){throw 'Fresh install accepts unsupported Windows'}
$taskSession.Property('WINDOWSBUILD')='26300'
if($taskSession.EvaluateCondition($taskCondition) -ne 1){throw 'Fresh install rejects compatible Windows x64'}
[void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($taskSession)
[void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($taskEngine)
$taskInstalled=Join-Path $taskWork 'application'
New-Item -ItemType Directory -Path $taskInstalled | Out-Null
$taskFiles=$taskXml.SelectNodes('//w:Directory[@Id="INSTALLFOLDER"]//w:File',$taskManager)
$taskNames=@()
foreach($taskFile in $taskFiles) {
    $taskParts=@();$taskParent=$taskFile.ParentNode.ParentNode
    while($taskParent.Id -ne 'INSTALLFOLDER') {
        if($taskParent.LocalName -ne 'Directory'){throw 'Unexpected MSI file layout'}
        $taskParts=@($taskParent.Name)+$taskParts;$taskParent=$taskParent.ParentNode
    }
    $taskRelative=(@($taskParts)+@($taskFile.Name)) -join '/'
    $taskNames+=$taskRelative
    if($taskRelative -match '(?i)\.pfx$|\.key$|\.wav$|\.user\.dps$|attack-body|intensity-aligned'){throw 'Private/experimental content in MSI'}
    $taskTarget=Join-Path $taskInstalled $taskRelative
    New-Item -ItemType Directory -Path (Split-Path $taskTarget -Parent) -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $taskWork ('extracted/File/'+$taskFile.Id)) -Destination $taskTarget
}
$taskRequired=if($Cable){@('resoniq.exe','resoniq.ico','installed.marker','VB-CABLE.url','VB-CABLE.txt','Prepare-VBCABLE.ps1','Install-VBCABLE.cmd')}else{@('resoniq.exe','resoniq_device.exe','installed.marker','driver/build/DpsAudioVirtual.sys','driver/build/DpsAudioVirtual.inf','driver/build/DpsAudioVirtual.cat','driver/build/MS-PL.txt')}
foreach($taskName in $taskRequired) {
    if($taskName -notin $taskNames){throw ('MSI missing: '+$taskName)}
}
if($Cable -and @($taskNames | Where-Object {$_ -match '(?i)\.sys$|\.cat$|\.cer$|Manage-Device|resoniq_device'}).Count){throw 'Cable MSI contains own driver components'}
if($Cable) {
    $taskPackageVersion=$taskXml.SelectSingleNode('//w:Package',$taskManager).Version
    if([Diagnostics.FileVersionInfo]::GetVersionInfo((Join-Path $taskInstalled 'resoniq.exe')).ProductVersion -ne $taskPackageVersion){throw 'Packaged application and MSI versions differ'}
    foreach($taskScript in @('Prepare-VBCABLE.ps1','Install-VBCABLE.cmd')) {
        if((Get-FileHash (Join-Path $taskInstalled $taskScript)).Hash -ne (Get-FileHash (Join-Path $taskRoot ('installer/'+$taskScript))).Hash){throw 'Packaged assistant differs from tested source'}
    }
    $taskPrepared=& "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File (Join-Path $taskInstalled 'Prepare-VBCABLE.ps1') -VerifyOnly -PackagePath (Join-Path $taskRoot 'build/vbcable-research/VBCABLE_Driver_Pack45.zip') -DataDirectory (Join-Path $taskWork 'prerequisite-test')
    if($LASTEXITCODE -ne 0){throw 'Packaged assistant verification failed'}
    $taskPrepared=$taskPrepared | ConvertFrom-Json
    if(-not $taskPrepared.verified -or $taskPrepared.installed){throw 'Packaged assistant verification result invalid'}
}
if(-not $Cable) {
$taskParserErrors=$null;$taskTokens=$null
[void][Management.Automation.Language.Parser]::ParseFile((Join-Path $taskInstalled 'driver/Manage-Device.ps1'),[ref]$taskTokens,[ref]$taskParserErrors)
if($taskParserErrors.Count){throw 'Invalid PowerShell in MSI'}
& (Join-Path $taskInstalled 'driver/verify_package.ps1')
}
$taskHelper=if($Cable){Join-Path $taskRoot 'build/Release/resoniq_device.exe'}else{Join-Path $taskInstalled 'resoniq_device.exe'}
$taskBefore=& $taskHelper --check
if($LASTEXITCODE -ne 0){throw 'Device preflight failed'}
$taskData=Join-Path $taskWork 'user-data'
$taskProfile=Join-Path $taskRoot 'config/session.user.dps'
$taskModes=if($SkipAudioTest){@('--self-test')}else{@('--self-test','--audio-test')}
foreach($taskMode in $taskModes) {
    $taskProcess=Start-Process -FilePath (Join-Path $taskInstalled 'resoniq.exe') -ArgumentList @($taskMode,'--data-dir',('"'+$taskData+'"'),'--profile',('"'+$taskProfile+'"')) -WindowStyle Hidden -PassThru
    if(-not $taskProcess.WaitForExit(20000)){throw ('Installed-layout timeout: '+$taskMode)}
    if($taskProcess.ExitCode -ne 0){throw ('Installed-layout failure: '+$taskMode)}
}
if(Test-Path -LiteralPath (Join-Path $taskInstalled 'config')){throw 'Application writes configuration beside Program Files binaries'}
if(-not (Test-Path -LiteralPath (Join-Path $taskData ('build/'+$(if($SkipAudioTest){'gui-test-result.txt'}else{'gui-audio-result.txt'}))))){throw 'User data redirection failed'}
$taskAfter=& $taskHelper --check
if($taskBefore -ne $taskAfter){throw 'Device preflight changed the system'}
if(-not $Cable) {
$taskProductionRejected=$false
try {& (Join-Path $taskRoot 'installer/Build-Installer.ps1') -Production}catch {$taskProductionRejected=$true}
if(-not $taskProductionRejected){throw 'Test-signed driver was accepted for production'}
}
Write-Output ('PASS: MSI payload, OS gate, prerequisites, driver isolation and installed layout GUI; Cable='+$Cable+'; audio_test='+$(if($SkipAudioTest){'SKIPPED'}else{'PASS'})+'; evidence: '+$taskWork)
