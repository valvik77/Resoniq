[CmdletBinding()]
param([string]$BinaryPath='build/installer-release/Release/resoniq.exe')
$ErrorActionPreference='Stop'
$taskRoot=Split-Path $PSScriptRoot -Parent
$taskPayload=Join-Path $taskRoot 'build/cable-installer-payload'
New-Item -ItemType Directory -Path $taskPayload,(Join-Path $taskPayload 'profiles'),(Join-Path $taskRoot 'dist') -Force | Out-Null
$taskFiles=@{
 'resoniq.exe'=Join-Path $taskRoot $BinaryPath
 'LICENSE'=Join-Path $taskRoot 'LICENSE'
 'THIRD_PARTY_NOTICES.md'=Join-Path $taskRoot 'THIRD_PARTY_NOTICES.md'
 'VB-CABLE.txt'=Join-Path $taskRoot 'docs/VB-CABLE.txt'
 'Prepare-VBCABLE.ps1'=Join-Path $PSScriptRoot 'Prepare-VBCABLE.ps1'
 'Install-VBCABLE.cmd'=Join-Path $PSScriptRoot 'Install-VBCABLE.cmd'
 'resoniq.ico'=Join-Path $taskRoot 'assets/brand/resoniq-app.ico'
}
foreach($taskName in @('external.dps','headphones.dps','laptop.dps','laptop-focus.dps','laptop-impact.dps','laptop-impact-plus.dps')){$taskFiles['profiles/'+$taskName]=Join-Path $taskRoot ('profiles/'+$taskName)}
$taskProduct=[xml](Get-Content (Join-Path $PSScriptRoot 'CableProduct.wxs') -Raw)
$taskVersion=$taskProduct.Wix.Package.Version
if([Diagnostics.FileVersionInfo]::GetVersionInfo($taskFiles['resoniq.exe']).ProductVersion -ne $taskVersion){throw 'La version de la aplicacion no coincide con la del MSI. Recompila antes de empaquetar.'}
$taskCloseDll=Join-Path (Split-Path $taskFiles['resoniq.exe'] -Parent) 'resoniq_close.dll'
$taskCloseText=[Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($taskCloseDll))
if($taskCloseText -match '(?i)(msvcp|vcruntime|concrt)\d+[^\x00]*\.dll'){throw 'El helper de cierre requiere runtime estatico'}
Copy-Item -LiteralPath $taskCloseDll -Destination (Join-Path $taskPayload 'resoniq_close.dll') -Force
$taskBinaryText=[Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($taskFiles['resoniq.exe']))
if($taskBinaryText -match '(?i)(msvcp|vcruntime|concrt)\d+[^\x00]*\.dll'){throw 'Compila con RESONIQ_STATIC_RUNTIME=ON'}
foreach($taskEntry in $taskFiles.GetEnumerator()){Copy-Item -LiteralPath $taskEntry.Value -Destination (Join-Path $taskPayload $taskEntry.Key) -Force}
Set-Content -LiteralPath (Join-Path $taskPayload 'installed.marker') -Value 'RESONIQ installed layout v1'
foreach($taskArt in @('installer-banner.bmp','installer-dialog.bmp')) {Copy-Item -LiteralPath (Join-Path $taskRoot ('assets/brand/'+$taskArt)) -Destination (Join-Path $taskPayload $taskArt) -Force}
Set-Content -LiteralPath (Join-Path $taskPayload 'VB-CABLE.url') -Value "[InternetShortcut]`r`nURL=https://vb-audio.com/Cable/"
$taskFiles['installed.marker']=Join-Path $taskPayload 'installed.marker';$taskFiles['VB-CABLE.url']=Join-Path $taskPayload 'VB-CABLE.url'
$taskLicense=(Get-Content -LiteralPath (Join-Path $taskRoot 'LICENSE') -Raw)+"`nVB-CABLE se instala por separado desde VB-Audio. Es donationware; consulta sus condiciones."
$taskLicense=$taskLicense.Replace('\','\\').Replace('{','\{').Replace('}','\}').Replace("`r",'').Replace("`n",'\par ')
Set-Content -LiteralPath (Join-Path $taskPayload 'license.rtf') -Value ('{\rtf1\ansi '+$taskLicense+'}') -Encoding ASCII
$taskXml=[xml]'<Wix xmlns="http://wixtoolset.org/schemas/v4/wxs"><Fragment><ComponentGroup Id="PayloadFiles" Directory="INSTALLFOLDER" /></Fragment></Wix>'
$taskNs=$taskXml.DocumentElement.NamespaceURI;$taskGroup=$taskXml.DocumentElement.FirstChild.FirstChild;$taskIndex=0
foreach($taskRelative in ($taskFiles.Keys | Sort-Object)) {
 $taskComponent=$taskXml.CreateElement('Component',$taskNs);$taskComponent.SetAttribute('Id',('CableFileComponent'+$taskIndex));$taskComponent.SetAttribute('Guid','*');$taskComponent.SetAttribute('Bitness','always64')
 $taskParent=Split-Path $taskRelative -Parent;if($taskParent){$taskComponent.SetAttribute('Subdirectory',$taskParent.Replace('/','\'))}
 $taskFile=$taskXml.CreateElement('File',$taskNs);$taskId=if($taskRelative -eq 'VB-CABLE.txt'){'CableInstructions'}elseif($taskRelative -eq 'Install-VBCABLE.cmd'){'CableLauncher'}elseif($taskRelative -eq 'resoniq.exe'){'ResoniqExecutable'}else{'CableFile'+$taskIndex};$taskFile.SetAttribute('Id',$taskId);$taskFile.SetAttribute('Source',(Join-Path $taskPayload $taskRelative));$taskFile.SetAttribute('KeyPath','yes')
 [void]$taskComponent.AppendChild($taskFile);[void]$taskGroup.AppendChild($taskComponent);$taskIndex++
}
$taskXml.Save((Join-Path $taskPayload 'Files.wxs'))
$taskMsi=Join-Path $taskRoot ('dist/RESONIQ-Setup-'+$taskVersion+'-x64-VBCABLE.msi')
& (Join-Path $taskRoot 'build/tools/wix/wix.exe') build (Join-Path $PSScriptRoot 'CableProduct.wxs') (Join-Path $taskPayload 'Files.wxs') -arch x64 -ext WixToolset.UI.wixext -ext WixToolset.Util.wixext -culture es-es -d ('Payload='+$taskPayload) -o $taskMsi
if($LASTEXITCODE -ne 0){throw 'Cable MSI build failed'}
Write-Output $taskMsi
