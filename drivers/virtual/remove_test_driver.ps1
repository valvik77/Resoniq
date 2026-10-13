[CmdletBinding()]
param([switch]$Remove)
$ErrorActionPreference = 'Stop'
if (-not $Remove) { throw 'Use -Remove to remove only the DPS virtual driver and certificates installed by its installer.' }
$taskPrincipal = [Security.Principal.WindowsPrincipal]::new([Security.Principal.WindowsIdentity]::GetCurrent())
if (-not $taskPrincipal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) { throw 'Run as administrator.' }
$taskDevices = @(Get-CimInstance Win32_PnPEntity | Where-Object { $_.HardwareID -contains 'ROOT\DPSAUDIOVIRTUAL' })
$taskInfNames = @(Get-CimInstance Win32_PnPSignedDriver | Where-Object { $_.DeviceID -in $taskDevices.DeviceID -and $_.DriverProviderName -eq 'DPS Audio Engine' } | Select-Object -ExpandProperty InfName -Unique)
$taskDevcon = Join-Path $PSScriptRoot 'packages\Microsoft.Windows.WDK.x64.10.0.28000.2526\c\tools\10.0.28000.0\x64\devcon.exe'
foreach ($taskDevice in $taskDevices) {
    & $taskDevcon remove ('@' + $taskDevice.DeviceID)
    if ($LASTEXITCODE -gt 1) { throw 'DPS device removal failed.' }
}
foreach ($taskInf in $taskInfNames) {
    if ($taskInf -notmatch '^oem\d+\.inf$') { throw 'Unexpected published INF name.' }
    & "$env:SystemRoot\System32\pnputil.exe" /delete-driver $taskInf
    if ($LASTEXITCODE -ne 0) { throw 'DPS driver package removal failed.' }
}
$taskStatePath = Join-Path $PSScriptRoot 'build\install-state.json'
if (Test-Path -LiteralPath $taskStatePath) {
    $taskState = Get-Content -LiteralPath $taskStatePath -Raw | ConvertFrom-Json
    if ($taskState.thumbprint -notmatch '^[A-Fa-f0-9]{40}$') { throw 'Unexpected certificate thumbprint.' }
    foreach ($taskStore in $taskState.added_certificate_stores) {
        if ($taskStore -notin @('Root','TrustedPublisher')) { throw 'Unexpected certificate store.' }
        $taskCertPath = "Cert:\LocalMachine\$taskStore\$($taskState.thumbprint)"
        if (Test-Path -LiteralPath $taskCertPath) { Remove-Item -LiteralPath $taskCertPath }
    }
}
Write-Output 'DPS device/package removal completed. TESTSIGNING and Windows default output were not changed.'
