[CmdletBinding()]
param([string]$BinaryDirectory='build/installer-release/Release', [string]$DriverDirectory='drivers/virtual/build', [switch]$Production)
$ErrorActionPreference='Stop'
$taskRoot=Split-Path $PSScriptRoot -Parent
$taskBinary=if([IO.Path]::IsPathRooted($BinaryDirectory)){[IO.Path]::GetFullPath($BinaryDirectory)}else{[IO.Path]::GetFullPath((Join-Path $taskRoot $BinaryDirectory))}
$taskDriver=if([IO.Path]::IsPathRooted($DriverDirectory)){[IO.Path]::GetFullPath($DriverDirectory)}else{[IO.Path]::GetFullPath((Join-Path $taskRoot $DriverDirectory))}
foreach($taskExecutable in @('resoniq.exe','resoniq_device.exe')) {
    $taskBinaryText=[Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes((Join-Path $taskBinary $taskExecutable)))
    if($taskBinaryText -match '(?i)(msvcp|vcruntime|concrt)\d+[^\x00]*\.dll'){throw 'Los binarios del instalador deben usar RESONIQ_STATIC_RUNTIME=ON'}
}
$taskWix=Join-Path $taskRoot 'build/tools/wix/wix.exe'
if(-not (Test-Path -LiteralPath $taskWix)){throw 'Instala WiX 6.0.2: dotnet tool install wix --version 6.0.2 --tool-path build/tools/wix'}
$taskMode=if($Production){'production'}else{'development'}
if($Production) {
    $taskSignature=Get-AuthenticodeSignature -LiteralPath (Join-Path $taskDriver 'DpsAudioVirtual.cat')
    if($taskSignature.Status -ne 'Valid' -or $taskSignature.SignerCertificate.Subject -notmatch 'Microsoft Windows Hardware Compatibility Publisher'){throw 'Produccion requiere catalogo firmado por Microsoft. El paquete de pruebas no se puede etiquetar como produccion.'}
    $taskSignTool=Join-Path $taskRoot 'drivers/virtual/packages/Microsoft.Windows.SDK.CPP.10.0.28000.2526/c/bin/10.0.28000.0/x64/signtool.exe'
    if(-not (Test-Path -LiteralPath $taskSignTool)){throw 'Restaura el SDK de compilacion para verificar la firma de produccion'}
    & $taskSignTool verify /kp /c (Join-Path $taskDriver 'DpsAudioVirtual.cat') (Join-Path $taskDriver 'DpsAudioVirtual.sys')
    if($LASTEXITCODE -ne 0){throw 'Firma kernel o pertenencia al catalogo no valida'}
    & $taskSignTool verify /pa /c (Join-Path $taskDriver 'DpsAudioVirtual.cat') (Join-Path $taskDriver 'DpsAudioVirtual.inf')
    if($LASTEXITCODE -ne 0){throw 'INF no coincide con el catalogo firmado'}
} else {
    & (Join-Path $taskRoot 'drivers/virtual/verify_package.ps1') -PackageDirectory $taskDriver
}
$taskPayload=Join-Path $taskRoot ('build/installer-payload-'+$taskMode)
New-Item -ItemType Directory -Force -Path $taskPayload,(Join-Path $taskPayload 'profiles'),(Join-Path $taskPayload 'driver'),(Join-Path $taskPayload 'driver/build') | Out-Null
$taskFiles=@{
    'resoniq.exe'=Join-Path $taskBinary 'resoniq.exe'
    'resoniq_device.exe'=Join-Path $taskBinary 'resoniq_device.exe'
    'LICENSE'=Join-Path $taskRoot 'LICENSE'
    'THIRD_PARTY_NOTICES.md'=Join-Path $taskRoot 'THIRD_PARTY_NOTICES.md'
    'INSTALLATION.md'=Join-Path $taskRoot 'docs/INSTALLATION.md'
    'driver/Manage-Device.ps1'=Join-Path $PSScriptRoot 'Manage-Device.ps1'
    'driver/build/DpsAudioVirtual.sys'=Join-Path $taskDriver 'DpsAudioVirtual.sys'
    'driver/build/DpsAudioVirtual.inf'=Join-Path $taskDriver 'DpsAudioVirtual.inf'
    'driver/build/DpsAudioVirtual.cat'=Join-Path $taskDriver 'DpsAudioVirtual.cat'
    'driver/build/MS-PL.txt'=Join-Path $taskDriver 'MS-PL.txt'
}
if(-not $Production) {
    $taskFiles['driver/build/DpsAudioDevelopment.cer']=Join-Path $taskDriver 'DpsAudioDevelopment.cer'
    foreach($taskScript in @('check_target.ps1','verify_package.ps1')){$taskFiles['driver/'+$taskScript]=Join-Path $taskRoot ('drivers/virtual/'+$taskScript)}
}
foreach($taskProfile in @('external.dps','headphones.dps','laptop.dps','laptop-focus.dps','laptop-impact.dps','laptop-impact-plus.dps')){$taskFiles['profiles/'+$taskProfile]=Join-Path $taskRoot ('profiles/'+$taskProfile)}
foreach($taskEntry in $taskFiles.GetEnumerator()){Copy-Item -LiteralPath $taskEntry.Value -Destination (Join-Path $taskPayload $taskEntry.Key) -Force}
Set-Content -LiteralPath (Join-Path $taskPayload 'installed.marker') -Value 'RESONIQ installed layout v1'
Set-Content -LiteralPath (Join-Path $taskPayload 'driver/build/signing-mode.txt') -Value $taskMode
$taskFiles['installed.marker']=Join-Path $taskPayload 'installed.marker'
$taskFiles['driver/build/signing-mode.txt']=Join-Path $taskPayload 'driver/build/signing-mode.txt'
$taskLicense=(Get-Content -LiteralPath (Join-Path $taskRoot 'LICENSE') -Raw).Replace('\','\\').Replace('{','\{').Replace('}','\}').Replace("`r",'').Replace("`n",'\par ')
Set-Content -LiteralPath (Join-Path $taskPayload 'license.rtf') -Value ('{\rtf1\ansi '+$taskLicense+'}') -Encoding ASCII
# Generate components from the same allowlist; no wildcard includes build files.
$taskFragment=[xml]'<Wix xmlns="http://wixtoolset.org/schemas/v4/wxs"><Fragment><ComponentGroup Id="PayloadFiles" Directory="INSTALLFOLDER" /></Fragment></Wix>'
$taskNamespace=$taskFragment.DocumentElement.NamespaceURI
$taskGroup=$taskFragment.DocumentElement.FirstChild.FirstChild
$taskIndex=0
foreach($taskRelative in ($taskFiles.Keys | Sort-Object)) {
    $taskComponent=$taskFragment.CreateElement('Component',$taskNamespace)
    $taskComponent.SetAttribute('Id',('FileComponent'+$taskIndex));$taskComponent.SetAttribute('Guid','*');$taskComponent.SetAttribute('Bitness','always64')
    $taskParent=Split-Path $taskRelative -Parent
    if($taskParent){$taskComponent.SetAttribute('Subdirectory',$taskParent.Replace('/','\'))}
    $taskFile=$taskFragment.CreateElement('File',$taskNamespace)
    $taskFile.SetAttribute('Id',('PayloadFile'+$taskIndex));$taskFile.SetAttribute('Source',(Join-Path $taskPayload $taskRelative));$taskFile.SetAttribute('KeyPath','yes')
    [void]$taskComponent.AppendChild($taskFile);[void]$taskGroup.AppendChild($taskComponent);$taskIndex++
}
$taskFragment.Save((Join-Path $taskPayload 'Files.wxs'))
$taskName=if($Production){'RESONIQ-Setup-0.2.0-x64'}else{'RESONIQ-Setup-0.2.0-x64-Development'}
$taskProductName=if($Production){'RESONIQ'}else{'RESONIQ - controlador de desarrollo'}
$taskDescription=if($Production){'Instala el dispositivo virtual DSP firmado por Microsoft.'}else{'Solo para equipos de pruebas ya configurados con TESTSIGNING y Secure Boot desactivado. Para equipos normales falta firma Microsoft.'}
$taskLevel=if($Production){'1'}else{'0'}
New-Item -ItemType Directory -Path (Join-Path $taskRoot 'dist') -Force | Out-Null
& $taskWix build (Join-Path $PSScriptRoot 'Product.wxs') (Join-Path $taskPayload 'Files.wxs') -arch x64 -ext WixToolset.UI.wixext -culture es-es -d ('Payload='+$taskPayload) -d ('ProductName='+$taskProductName) -d ('DriverDescription='+$taskDescription) -d ('DriverLevel='+$taskLevel) -d ('SigningMode='+$taskMode) -o (Join-Path $taskRoot ('dist/'+$taskName+'.msi'))
if($LASTEXITCODE -ne 0){throw 'MSI build failed'}
Write-Output (Join-Path $taskRoot ('dist/'+$taskName+'.msi'))
