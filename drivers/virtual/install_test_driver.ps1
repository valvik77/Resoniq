# Run after a manual reboot into the explicitly authorized test-signing mode.
[CmdletBinding()]
param([switch]$Install)
$ErrorActionPreference = 'Stop'
if (-not $Install) { throw 'Use -Install to install the reviewed development package.' }
$taskBuild = Join-Path $PSScriptRoot 'build'
& (Join-Path $PSScriptRoot 'check_target.ps1')
$taskPreflight = Get-Content -LiteralPath (Join-Path $taskBuild 'target-preflight.json') -Raw | ConvertFrom-Json
if (-not $taskPreflight.eligible_for_test_driver) { throw 'Requires administrator, Secure Boot off and TESTSIGNING active in the current running kernel. Reboot after configuring TESTSIGNING.' }
& (Join-Path $PSScriptRoot 'verify_package.ps1')
$taskCertPath = Join-Path $taskBuild 'DpsAudioDevelopment.cer'
$taskCert = [Security.Cryptography.X509Certificates.X509Certificate2]::new($taskCertPath)
$taskThumbprint = $taskCert.Thumbprint
$taskAdded = @()
$taskStatePath = Join-Path $taskBuild 'install-state.json'
if (Test-Path -LiteralPath $taskStatePath) {
    $taskPrior = Get-Content -LiteralPath $taskStatePath -Raw | ConvertFrom-Json
    if ($taskPrior.thumbprint -eq $taskThumbprint) { $taskAdded = @($taskPrior.added_certificate_stores) }
}
foreach ($taskStoreName in @('Root','TrustedPublisher')) {
    if (-not (Test-Path -LiteralPath "Cert:\LocalMachine\$taskStoreName\$taskThumbprint")) {
        [void](Import-Certificate -FilePath $taskCertPath -CertStoreLocation "Cert:\LocalMachine\$taskStoreName")
        if ($taskStoreName -notin $taskAdded) { $taskAdded += $taskStoreName }
    }
}
$taskDevcon = Join-Path $PSScriptRoot 'packages\Microsoft.Windows.WDK.x64.10.0.28000.2526\c\tools\10.0.28000.0\x64\devcon.exe'
$taskExisting = @(Get-CimInstance Win32_PnPEntity | Where-Object { $_.HardwareID -contains 'ROOT\DPSAUDIOVIRTUAL' })
$taskAction = if ($taskExisting.Count) { 'update' } else { 'install' }
& $taskDevcon $taskAction (Join-Path $taskBuild 'DpsAudioVirtual.inf') 'ROOT\DPSAUDIOVIRTUAL'
$taskResult = $LASTEXITCODE
@{thumbprint=$taskThumbprint; added_certificate_stores=$taskAdded; devcon_exit=$taskResult; hardware_id='ROOT\DPSAUDIOVIRTUAL'; changed_default_output=$false} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $taskBuild 'install-state.json') -Encoding utf8
$taskCert.Dispose()
if ($taskResult -ne 0 -and $taskResult -ne 1) { throw "Driver installation failed (devcon exit $taskResult). See Windows INF setup logs." }
if ($taskResult -eq 1) { Write-Output 'Driver installation requires a manual reboot.' }
Write-Output 'Installation attempted. Verify device status and WASAPI enumeration before routing audio. The default Windows output has not been changed.'
