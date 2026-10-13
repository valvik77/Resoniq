[CmdletBinding()]
param([switch]$Install,[switch]$Remove,[switch]$OnlyIfOwned,[switch]$Rollback,[switch]$NonInteractive,[switch]$Check)
$ErrorActionPreference='Stop'
$taskRoot=Split-Path $PSScriptRoot -Parent
$taskHelper=Join-Path $taskRoot 'resoniq_device.exe'
$taskBuild=Join-Path $PSScriptRoot 'build'
$taskStateDirectory=Join-Path $env:ProgramData 'RESONIQ'
$taskStatePath=Join-Path $taskStateDirectory 'driver-state.json'
$taskPrincipal=[Security.Principal.WindowsPrincipal]::new([Security.Principal.WindowsIdentity]::GetCurrent())
if(-not $Check -and -not $taskPrincipal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    if($NonInteractive){throw 'La instalacion del dispositivo DSP requiere administrador'}
    $taskAction=if($Remove){'-Remove'}else{'-Install'}
    $taskChild=Start-Process -FilePath "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" -Verb RunAs -WindowStyle Hidden -PassThru -Wait -ArgumentList @('-NoProfile','-ExecutionPolicy','Bypass','-File',('"'+$PSCommandPath+'"'),$taskAction)
    exit $taskChild.ExitCode
}
if($Check){& $taskHelper --check;exit $LASTEXITCODE}
if($Install -eq $Remove){throw 'Selecciona Install o Remove'}
New-Item -ItemType Directory -Path $taskStateDirectory -Force | Out-Null
Start-Transcript -Path (Join-Path $taskStateDirectory 'driver-install.log') -Append | Out-Null
$taskAdded=@();$taskThumbprint=$null;$taskCompleted=$false
try {
    $taskPrior=$null
    if(Test-Path -LiteralPath $taskStatePath){$taskPrior=Get-Content -LiteralPath $taskStatePath -Raw | ConvertFrom-Json}
    if($Remove) {
        if($OnlyIfOwned -and (-not $taskPrior -or -not $taskPrior.device_owned)){exit 0}
        if($Rollback -and (-not $taskPrior -or -not $taskPrior.created_in_last_attempt)){exit 0}
        & $taskHelper --remove
        $taskExit=$LASTEXITCODE
        if($taskExit -notin @(0,3010)){throw 'No se pudo retirar el dispositivo DSP'}
        if($taskPrior -and $taskPrior.thumbprint -match '^[A-Fa-f0-9]{40}$') {
            foreach($taskStore in $taskPrior.added_certificate_stores) {
                if($taskStore -notin @('Root','TrustedPublisher')){throw 'Almacen de certificado no valido'}
                $taskCertificatePath="Cert:\LocalMachine\$taskStore\$($taskPrior.thumbprint)"
                if(Test-Path -LiteralPath $taskCertificatePath){Remove-Item -LiteralPath $taskCertificatePath}
            }
        }
        if(Test-Path -LiteralPath $taskStatePath){Remove-Item -LiteralPath $taskStatePath}
        if(-not $NonInteractive){Add-Type -AssemblyName System.Windows.Forms;[void][Windows.Forms.MessageBox]::Show('Dispositivo DSP retirado. Tus perfiles se conservan.','RESONIQ')}
        if($NonInteractive){exit 0};exit $taskExit
    }
    $taskBeforeText=& $taskHelper --check
    if($LASTEXITCODE -ne 0){throw 'Arquitectura no compatible'}
    $taskBefore=$taskBeforeText | ConvertFrom-Json
    $taskBuildNumber=[int](Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion').CurrentBuildNumber
    if($taskBuildNumber -lt 22621){throw 'Este controlador requiere Windows 11 22H2 o posterior, x64'}
    $taskMode=(Get-Content -LiteralPath (Join-Path $taskBuild 'signing-mode.txt') -Raw).Trim()
    if($taskMode -eq 'development') {
        & (Join-Path $PSScriptRoot 'check_target.ps1') | Out-Null
        $taskPreflight=Get-Content -LiteralPath (Join-Path $taskBuild 'target-preflight.json') -Raw | ConvertFrom-Json
        if(-not $taskPreflight.eligible_for_test_driver){throw 'Controlador de desarrollo: requiere modo TESTSIGNING ya activo y Secure Boot desactivado. Para un equipo normal hace falta el controlador firmado por Microsoft. No se ha modificado el arranque.'}
        & (Join-Path $PSScriptRoot 'verify_package.ps1')
        $taskCertificatePath=Join-Path $taskBuild 'DpsAudioDevelopment.cer'
        $taskCertificate=[Security.Cryptography.X509Certificates.X509Certificate2]::new($taskCertificatePath)
        $taskThumbprint=$taskCertificate.Thumbprint;$taskCertificate.Dispose()
        foreach($taskStore in @('Root','TrustedPublisher')) {
            if(-not (Test-Path -LiteralPath "Cert:\LocalMachine\$taskStore\$taskThumbprint")) {
                [void](Import-Certificate -FilePath $taskCertificatePath -CertStoreLocation "Cert:\LocalMachine\$taskStore")
                $taskAdded+=$taskStore
            }
        }
    } elseif($taskMode -eq 'production') {
        $taskSignature=Get-AuthenticodeSignature -LiteralPath (Join-Path $taskBuild 'DpsAudioVirtual.cat')
        if($taskSignature.Status -ne 'Valid' -or $taskSignature.SignerCertificate.Subject -notmatch 'Microsoft Windows Hardware Compatibility Publisher'){throw 'El catalogo no tiene la firma de distribucion Microsoft esperada'}
    } else {throw 'Modo de firma desconocido'}
    & $taskHelper --install $taskBuild
    $taskExit=$LASTEXITCODE
    if($taskExit -notin @(0,3010)){throw 'Fallo al instalar el dispositivo DSP; consulta driver-install.log y setupapi.dev.log'}
    $taskOwned=$taskBefore.dps_devices -eq 0 -or ($taskPrior -and $taskPrior.device_owned)
    $taskStores=@($taskAdded)
    if($taskPrior -and $taskPrior.thumbprint -eq $taskThumbprint){$taskStores+=@($taskPrior.added_certificate_stores)}
    @{device_owned=[bool]$taskOwned;created_in_last_attempt=($taskBefore.dps_devices -eq 0);thumbprint=$taskThumbprint;added_certificate_stores=@($taskStores | Select-Object -Unique);reboot_required=($taskExit -eq 3010)} | ConvertTo-Json | Set-Content -LiteralPath $taskStatePath -Encoding UTF8
    $taskCompleted=$true
    if(-not $NonInteractive){Add-Type -AssemblyName System.Windows.Forms;$taskMessage=if($taskExit -eq 3010){'Dispositivo DSP preparado. Reinicia Windows manualmente para completar la instalacion.'}else{'Dispositivo virtual DSP instalado. Abre RESONIQ y selecciona la salida fisica; despues elige DPS Audio Virtual Transport como salida Windows.'};[void][Windows.Forms.MessageBox]::Show($taskMessage,'RESONIQ')}
    if($NonInteractive){exit 0};exit $taskExit
} catch {
    Write-Output $_
    if(-not $NonInteractive){Add-Type -AssemblyName System.Windows.Forms;[void][Windows.Forms.MessageBox]::Show($_.Exception.Message,'RESONIQ - dispositivo DSP')}
    exit 1
} finally {
    if(-not $taskCompleted -and $taskThumbprint) {
        foreach($taskStore in $taskAdded){$taskCertificatePath="Cert:\LocalMachine\$taskStore\$taskThumbprint";if(Test-Path -LiteralPath $taskCertificatePath){Remove-Item -LiteralPath $taskCertificatePath}}
    }
    Stop-Transcript | Out-Null
}
