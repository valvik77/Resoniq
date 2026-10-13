[CmdletBinding()]
param([switch]$VerifyOnly,[switch]$PreviewOnly,[switch]$PreparePreview,[string]$PackagePath,[string]$DataDirectory)
$ErrorActionPreference='Stop'
# Nested Windows PowerShell processes can inherit a PowerShell 7 module path.
# Load inbox modules explicitly instead of relying on inherited auto-loading.
foreach($taskModule in @('Microsoft.PowerShell.Utility','Microsoft.PowerShell.Management','Microsoft.PowerShell.Security','Microsoft.PowerShell.Archive','CimCmdlets')) {
    Import-Module -Name (Join-Path $PSHOME ('Modules/'+$taskModule+'/'+$taskModule+'.psd1')) -Force
}
$taskExpectedHash='B950E39F01AF1D04EA623C8F6D8EB9B6EA5C477C637295FABF20631C85116BFB'
$taskSetupHash='734C35DFA6D98F48782A451633CEB471166EC70D60482FD89A1123D0EE3C4F41'
$taskUrl='https://download.vb-audio.com/Download_CABLE/VBCABLE_Driver_Pack45.zip'
$taskSelfPath=$PSCommandPath
function Prepare-CablePackage {
    if(-not [Environment]::Is64BitOperatingSystem -or $env:PROCESSOR_ARCHITECTURE -ne 'AMD64'){throw 'Este asistente requiere Windows x64.'}
    $taskBase=if($DataDirectory){[IO.Path]::GetFullPath($DataDirectory)}else{Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'RESONIQ/prerequisites'}
    $taskStage=Join-Path $taskBase ('vbcable-'+[Guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $taskStage -Force | Out-Null
    $taskZip=Join-Path $taskStage 'VBCABLE_Driver_Pack45.zip'
    if($PackagePath){Copy-Item -LiteralPath $PackagePath -Destination $taskZip}
    else {
        [Net.ServicePointManager]::SecurityProtocol=[Net.SecurityProtocolType]::Tls12
        Invoke-WebRequest -UseBasicParsing -Uri $taskUrl -OutFile $taskZip -TimeoutSec 60
    }
    if((Get-FileHash -LiteralPath $taskZip -Algorithm SHA256).Hash -ne $taskExpectedHash){throw 'El paquete descargado no coincide con la version verificada. No se ejecutara; consulta la descarga oficial de VB-Audio.'}
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $taskArchive=[IO.Compression.ZipFile]::OpenRead($taskZip)
    $taskExtract=Join-Path $taskStage 'package'
    try {
        $taskSize=0
        foreach($taskEntry in $taskArchive.Entries) {
            $taskResolved=[IO.Path]::GetFullPath((Join-Path $taskExtract $taskEntry.FullName))
            if(-not $taskResolved.StartsWith([IO.Path]::GetFullPath($taskExtract)+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Ruta de paquete no valida'}
            $taskSize+=$taskEntry.Length
        }
        if($taskSize -gt 64MB){throw 'Tamano de paquete inesperado'}
    }finally{$taskArchive.Dispose()}
    Expand-Archive -LiteralPath $taskZip -DestinationPath $taskExtract
    $taskSetup=Join-Path $taskExtract 'VBCABLE_Setup_x64.exe'
    foreach($taskName in @('VBCABLE_Setup_x64.exe','vbaudio_cable64_win10.cat','vbaudio_cable64_win10.sys')) {
        $taskSignature=Get-AuthenticodeSignature -LiteralPath (Join-Path $taskExtract $taskName)
        if($taskSignature.Status -ne 'Valid'){throw ('Firma no valida: '+$taskName)}
        $taskPublisher=if($taskName -eq 'VBCABLE_Setup_x64.exe'){'BUREL VINCENT'}else{'Microsoft Windows Hardware Compatibility Publisher'}
        if($taskSignature.SignerCertificate.Subject -notmatch $taskPublisher){throw ('Editor inesperado: '+$taskName)}
    }
    [pscustomobject]@{setup=$taskSetup;package=$taskZip;sha256=$taskExpectedHash;verified=$true;installed=$false}
}
function Find-CableDevice {
    @(Get-CimInstance Win32_PnPEntity | Where-Object {
        @($_.HardwareID | Where-Object {$_ -match '^(ROOT\\)?VBAudioVACWDM$'}).Count -gt 0
    })
}
if($VerifyOnly){Prepare-CablePackage | ConvertTo-Json;exit 0}
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
[Windows.Forms.Application]::EnableVisualStyles()
$taskForm=New-Object Windows.Forms.Form
$taskIconPath=Join-Path $PSScriptRoot 'resoniq.ico'
if(-not (Test-Path -LiteralPath $taskIconPath)){$taskIconPath=Join-Path $PSScriptRoot '../assets/brand/resoniq-app.ico'}
if(Test-Path -LiteralPath $taskIconPath){$taskForm.Icon=New-Object Drawing.Icon($taskIconPath)}
$taskForm.Text='RESONIQ - Instalar VB-CABLE';$taskForm.ClientSize=New-Object Drawing.Size(570,250)
$taskForm.FormBorderStyle='FixedDialog';$taskForm.MaximizeBox=$false;$taskForm.StartPosition='CenterScreen'
$taskInfo=New-Object Windows.Forms.Label
$taskInfo.Location=New-Object Drawing.Point(20,20);$taskInfo.Size=New-Object Drawing.Size(530,78)
$taskInfo.Text="VB-CABLE es de VB-Audio y se distribuye como donationware.`r`nEste asistente descarga el paquete oficial y abre su instalador.`r`nEl uso profesional tiene requisitos de licencia adicionales."
$taskForm.Controls.Add($taskInfo)
$taskLicense=New-Object Windows.Forms.LinkLabel
$taskLicense.Location=New-Object Drawing.Point(20,100);$taskLicense.Size=New-Object Drawing.Size(530,24);$taskLicense.Text='Consultar condiciones de VB-Audio'
$taskLicense.Add_LinkClicked({Start-Process 'https://vb-audio.com/Services/licensing.htm'})
$taskForm.Controls.Add($taskLicense)
$taskStatus=New-Object Windows.Forms.Label
$taskStatus.Location=New-Object Drawing.Point(20,135);$taskStatus.Size=New-Object Drawing.Size(530,50)
$taskStatus.Text='Windows pedira permiso de administrador. El reinicio sera manual.'
$taskForm.Controls.Add($taskStatus)
$taskInstall=New-Object Windows.Forms.Button
$taskInstall.Location=New-Object Drawing.Point(20,200);$taskInstall.Size=New-Object Drawing.Size(290,32);$taskInstall.Text='Descargar y abrir instalador oficial'
$taskForm.Controls.Add($taskInstall)
$taskClose=New-Object Windows.Forms.Button
$taskClose.Location=New-Object Drawing.Point(440,200);$taskClose.Size=New-Object Drawing.Size(110,32);$taskClose.Text='Cerrar';$taskClose.Add_Click({$taskForm.Close()})
$taskForm.Controls.Add($taskClose)
$script:taskSetupProcess=$null
$script:taskDownloadProcess=$null
$script:taskDownloadRoot=$null
$taskTimer=New-Object Windows.Forms.Timer;$taskTimer.Interval=500
$taskTimer.Add_Tick({
    if($script:taskDownloadProcess -and $script:taskDownloadProcess.HasExited) {
        try {
            $script:taskDownloadProcess.WaitForExit()
            if($script:taskDownloadProcess.ExitCode -ne 0){
                $taskFailure=Get-Content -LiteralPath (Join-Path $script:taskDownloadRoot 'error.txt') -Raw
                throw ('No se pudo preparar VB-CABLE. '+$taskFailure)
            }
            $taskPrepared=Get-Content -LiteralPath (Join-Path $script:taskDownloadRoot 'result.json') -Raw | ConvertFrom-Json
            if(-not $taskPrepared.verified -or $taskPrepared.sha256 -ne $taskExpectedHash){throw 'Resultado de preparacion no valido'}
            $taskSignature=Get-AuthenticodeSignature -LiteralPath $taskPrepared.setup
            if($taskSignature.Status -ne 'Valid' -or $taskSignature.SignerCertificate.Subject -notmatch 'BUREL VINCENT'){throw 'Firma del instalador no valida'}
            if((Get-FileHash -LiteralPath $taskPrepared.setup).Hash -ne $taskSetupHash){throw 'El instalador preparado ha cambiado'}
            if($PreparePreview) {
                $taskStatus.Text='Paquete verificado. Prueba completada sin instalar controlador.'
                $taskTimer.Stop()
                $taskPrepared | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $DataDirectory 'prepare-result.json') -Encoding UTF8
                $taskForm.Close();return
            }
            $taskStatus.Text='En el asistente de VB-Audio pulsa Install Driver si aceptas sus condiciones.'
            $script:taskSetupProcess=Start-Process -FilePath $taskPrepared.setup -WorkingDirectory (Split-Path $taskPrepared.setup -Parent) -Verb RunAs -WindowStyle Normal -PassThru
        }catch {
            $taskTimer.Stop();$taskInstall.Enabled=$true;$taskStatus.Text='No se ha completado la preparacion.'
            if($PreparePreview){$_.Exception.Message | Set-Content -LiteralPath (Join-Path $DataDirectory 'prepare-failure.txt');$taskForm.Close();return}
            [void][Windows.Forms.MessageBox]::Show($_.Exception.Message,'RESONIQ - VB-CABLE')
        }finally{$script:taskDownloadProcess.Dispose();$script:taskDownloadProcess=$null}
    }
    if($script:taskSetupProcess -and $script:taskSetupProcess.HasExited){
        $taskTimer.Stop();$taskInstall.Enabled=$true
        $taskStatus.Text='Si has instalado VB-CABLE, reinicia Windows manualmente. Despues selecciona CABLE Input en RESONIQ y comprueba tu microfono predeterminado.'
        $script:taskSetupProcess.Dispose();$script:taskSetupProcess=$null
    }
})
$taskInstall.Add_Click({
    try {
        $taskInstall.Enabled=$false
        if(-not $PreparePreview -and (Find-CableDevice).Count -gt 0){
            $taskStatus.Text='Windows ya detecta VB-CABLE. No se abre su instalador para evitar retirarlo. Selecciona CABLE Input en RESONIQ; reinicia si acabas de instalarlo.'
            $taskInstall.Enabled=$true;return
        }
        $taskStatus.Text='Descargando y comprobando el paquete oficial...';$taskForm.Refresh()
        $taskBase=if($DataDirectory){$DataDirectory}else{Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'RESONIQ/prerequisites'}
        $script:taskDownloadRoot=Join-Path $taskBase ('request-'+[Guid]::NewGuid().ToString('N'))
        New-Item -ItemType Directory -Path $script:taskDownloadRoot -Force | Out-Null
        $taskArguments=@('-NoProfile','-ExecutionPolicy','Bypass','-File',('"'+$taskSelfPath+'"'),'-VerifyOnly','-DataDirectory',('"'+$script:taskDownloadRoot+'"'))
        if($PackagePath){$taskArguments+=@('-PackagePath',('"'+[IO.Path]::GetFullPath($PackagePath)+'"'))}
        $script:taskDownloadProcess=Start-Process -FilePath "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" -ArgumentList $taskArguments -WindowStyle Hidden -RedirectStandardOutput (Join-Path $script:taskDownloadRoot 'result.json') -RedirectStandardError (Join-Path $script:taskDownloadRoot 'error.txt') -PassThru
        $null=$script:taskDownloadProcess.Handle
        $taskTimer.Start()
    }catch {
        $taskInstall.Enabled=$true;$taskStatus.Text='No se ha completado la preparacion.'
        if($PreparePreview){$_.Exception.Message | Set-Content -LiteralPath (Join-Path $DataDirectory 'prepare-failure.txt');$taskForm.Close();return}
        [void][Windows.Forms.MessageBox]::Show($_.Exception.Message,'RESONIQ - VB-CABLE')
    }
})
if($PreparePreview){$taskForm.Add_Shown({$taskInstall.PerformClick()})}
if($PreviewOnly) {
    $taskPreviewTimer=New-Object Windows.Forms.Timer;$taskPreviewTimer.Interval=600
    $taskPreviewTimer.Add_Tick({
        $taskPreviewTimer.Stop()
        $taskPreviewRoot=if($DataDirectory){$DataDirectory}else{Join-Path $PSScriptRoot '../build/vbcable-automation-preview'}
        New-Item -ItemType Directory -Path $taskPreviewRoot -Force | Out-Null
        $taskBitmap=New-Object Drawing.Bitmap($taskForm.Width,$taskForm.Height)
        $taskForm.DrawToBitmap($taskBitmap,(New-Object Drawing.Rectangle(0,0,$taskForm.Width,$taskForm.Height)))
        $taskBitmap.Save((Join-Path $taskPreviewRoot 'assistant.png'),[Drawing.Imaging.ImageFormat]::Png)
        $taskBitmap.Dispose();$taskForm.Close()
    });$taskPreviewTimer.Start()
}
try {[void]$taskForm.ShowDialog()}finally{
    if($script:taskDownloadProcess) {
        if(-not $script:taskDownloadProcess.HasExited){$script:taskDownloadProcess.Kill()}
        $script:taskDownloadProcess.Dispose()
    }
    $taskTimer.Stop();$taskTimer.Dispose();$taskForm.Dispose()
    if($PreviewOnly){$taskPreviewTimer.Dispose()}
}
