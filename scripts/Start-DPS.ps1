[CmdletBinding()]
param(
    [string]$OutputId,
    [string]$Profile,
    [ValidateSet('Natural','Enhanced','Powerful','Laptop Max','RESONIQ Natural','RESONIQ Enhance','RESONIQ Power','RESONIQ Max')][string]$Preset,
    [ValidateSet('Laptop','Laptop Focus','Laptop Impact','Laptop Impact+','Headphones','External')][string]$DeviceProfile,
    [ValidateRange(0,100)][int]$OutputVolume,
    [switch]$ChooseOutput
)
$ErrorActionPreference='Stop'
$taskRoot=Split-Path $PSScriptRoot -Parent
$taskExecutable=Join-Path $taskRoot 'build\Release\dps_live.exe'
if (-not (Test-Path -LiteralPath $taskExecutable)) { throw 'Compile the Release configuration first.' }
if (-not $Profile -and -not $Preset -and -not $DeviceProfile -and -not $OutputId -and -not $ChooseOutput) {
    foreach ($taskCandidate in @('config\current.user.dps','config\lenovo.user.dps')) {
        $taskPath=Join-Path $taskRoot $taskCandidate
        if (Test-Path -LiteralPath $taskPath) { $Profile=$taskPath;break }
    }
}
$taskArguments=@('--dps','--log',(Join-Path $taskRoot 'build\live-telemetry.log'))
if ($Profile) {
    $taskArguments+=@('--profile',(Resolve-Path -LiteralPath $Profile).Path)
} else {
    if (-not $Preset) { $Preset='Laptop Max' }
    if (-not $DeviceProfile) { $DeviceProfile='Laptop' }
    $taskArguments+=@('--raw')
    if (-not $OutputId -and -not $ChooseOutput) {
        $taskConfig=Join-Path $taskRoot 'config\local-routing.json'
        if (-not (Test-Path -LiteralPath $taskConfig)) { $ChooseOutput=$true }
        else { $OutputId=(Get-Content -LiteralPath $taskConfig -Raw | ConvertFrom-Json).output_id }
    }
}
if ($ChooseOutput) {
    & $taskExecutable --list
    $taskIndex=Read-Host 'Numero de la salida fisica (render)'
    $taskArguments+=@('--render',$taskIndex)
} elseif ($OutputId) { $taskArguments+=@('--render-id',$OutputId) }
if ($Preset) { $taskArguments+=@('--preset',$Preset) }
if ($DeviceProfile) { $taskArguments+=@('--device-profile',$DeviceProfile) }
if ($PSBoundParameters.ContainsKey('OutputVolume')) { $taskArguments+=@('--output-volume',"$OutputVolume") }
Write-Output 'Ruta: dispositivo virtual DPS -> motor DSP -> salida fisica.'
Write-Output 'Selecciona Altavoces (DPS Audio Virtual Transport) como salida de Windows.'
Push-Location $taskRoot
try {
    & $taskExecutable @taskArguments
    if ($LASTEXITCODE -ne 0) { throw "Audio host exited with code $LASTEXITCODE" }
} finally { Pop-Location }
