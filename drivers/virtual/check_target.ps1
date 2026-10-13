# Read-only deployment preflight. It never changes boot settings or installs drivers.
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$taskIdentity = [Security.Principal.WindowsIdentity]::GetCurrent()
$taskPrincipal = [Security.Principal.WindowsPrincipal]::new($taskIdentity)
$taskAdmin = $taskPrincipal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
$taskSecureBoot = $null
try { $taskSecureBoot = Confirm-SecureBootUEFI } catch {
    try { $taskSecureBoot = [bool](Get-ItemProperty 'HKLM:\SYSTEM\CurrentControlSet\Control\SecureBoot\State').UEFISecureBootEnabled } catch {}
}
$taskBcd = 'Unavailable without administrator privileges'
$taskTestSigning = $null
if (-not ('DpsCodeIntegrity' -as [type])) { Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class DpsCodeIntegrity {
    [StructLayout(LayoutKind.Sequential)] public struct Info { public uint Length; public uint Options; }
    [DllImport("ntdll.dll")] static extern int NtQuerySystemInformation(int cls, ref Info info, int size, out int returned);
    public static bool TestSigning() {
        Info info = new Info { Length = 8 }; int returned;
        int status = NtQuerySystemInformation(103, ref info, 8, out returned);
        if (status != 0) throw new InvalidOperationException("Code integrity query failed: " + status);
        return (info.Options & 2) != 0;
    }
}
'@
}
$taskRunningTestSigning = [DpsCodeIntegrity]::TestSigning()
if ($taskAdmin) {
    $taskStart = [Diagnostics.ProcessStartInfo]::new()
    $taskStart.FileName = "$env:SystemRoot\System32\bcdedit.exe"
    $taskStart.Arguments = '/enum {current}'
    $taskStart.UseShellExecute = $false
    $taskStart.CreateNoWindow = $true
    $taskStart.RedirectStandardOutput = $true
    $taskStart.RedirectStandardError = $true
    $taskProcess = [Diagnostics.Process]::Start($taskStart)
    $taskBcd = $taskProcess.StandardOutput.ReadToEnd()
    $taskError = $taskProcess.StandardError.ReadToEnd()
    $taskProcess.WaitForExit()
    if ($taskProcess.ExitCode -eq 0) {
        $taskTestSigning = [bool]($taskBcd -match '(?im)^\s*testsigning\s+(Yes|S[ií])\s*$')
    } else { $taskBcd = $taskError }
    $taskProcess.Dispose()
}
$taskInfo = [ordered]@{
    administrator=$taskAdmin
    secure_boot=$taskSecureBoot
    test_signing=$taskTestSigning
    running_test_signing=$taskRunningTestSigning
    bcd=$taskBcd
    eligible_for_test_driver=($taskAdmin -and $taskRunningTestSigning -eq $true -and $taskSecureBoot -eq $false)
    modified_boot=$false
    installed_driver=$false
}
$taskJson = $taskInfo | ConvertTo-Json
$taskJson | Set-Content -LiteralPath (Join-Path $PSScriptRoot 'build\target-preflight.json') -Encoding utf8
Write-Output $taskJson
