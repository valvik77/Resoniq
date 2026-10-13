# The user explicitly authorized TESTSIGNING for this Lenovo. No automatic reboot.
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$taskPrincipal = [Security.Principal.WindowsPrincipal]::new([Security.Principal.WindowsIdentity]::GetCurrent())
if (-not $taskPrincipal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) { throw 'Run as administrator.' }
if (Confirm-SecureBootUEFI) { throw 'Secure Boot is enabled. This script will not change it.' }
$taskStart = [Diagnostics.ProcessStartInfo]::new()
$taskStart.FileName = "$env:SystemRoot\System32\bcdedit.exe"
$taskStart.Arguments = '/set {current} testsigning on'
$taskStart.UseShellExecute = $false
$taskStart.CreateNoWindow = $true
$taskStart.RedirectStandardOutput = $true
$taskStart.RedirectStandardError = $true
$taskProcess = [Diagnostics.Process]::Start($taskStart)
$taskOutput = $taskProcess.StandardOutput.ReadToEnd()
$taskError = $taskProcess.StandardError.ReadToEnd()
$taskProcess.WaitForExit()
$taskResult = @{ exit_code=$taskProcess.ExitCode; output=$taskOutput; error=$taskError; automatic_reboot=$false }
$taskResult | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $PSScriptRoot 'build\enable-test-signing-result.json') -Encoding utf8
$taskExit = $taskProcess.ExitCode
$taskProcess.Dispose()
if ($taskExit -ne 0) { throw "TESTSIGNING configuration failed: $taskError" }
Write-Output 'TESTSIGNING configured. Reboot manually before installing the development driver.'
& (Join-Path $PSScriptRoot 'check_target.ps1')
