# Elevated entry point with persistent logs for the authorized local deployment.
$ErrorActionPreference = 'Stop'
$taskLog = Join-Path $PSScriptRoot 'build\deployment.log'
Start-Transcript -Path $taskLog -Force | Out-Null
try {
    & (Join-Path $PSScriptRoot 'install_test_driver.ps1') -Install
    @{success=$true; completed=(Get-Date).ToString('o')} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $PSScriptRoot 'build\deployment-result.json') -Encoding utf8
} catch {
    @{success=$false; error=$_.Exception.Message; completed=(Get-Date).ToString('o')} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $PSScriptRoot 'build\deployment-result.json') -Encoding utf8
    Write-Output $_
} finally { Stop-Transcript | Out-Null }
