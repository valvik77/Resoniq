[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$PackageDirectory)
$ErrorActionPreference='Stop'
$taskSource=[IO.Path]::GetFullPath($PackageDirectory)
$taskCopy=Join-Path ([IO.Path]::GetTempPath()) ('resoniq-portable-test-'+[Guid]::NewGuid().ToString('N'))
Copy-Item -LiteralPath $taskSource -Destination $taskCopy -Recurse
& (Join-Path $taskCopy 'scripts/Verify-Portable.ps1')
$taskProfile=Join-Path $taskCopy 'config/session.user.dps'
$taskOriginal=[IO.File]::ReadAllBytes($taskProfile)
[IO.File]::AppendAllText($taskProfile,"`n; altered`n")
$taskRejected=$false
try {& (Join-Path $taskCopy 'scripts/Verify-Portable.ps1')}catch {$taskRejected=$true}
if(-not $taskRejected){throw 'Modified package accepted'}
[IO.File]::WriteAllBytes($taskProfile,$taskOriginal)
$taskExecutable=Join-Path $taskCopy 'bin/resoniq.exe'
foreach($taskMode in @('--self-test','--audio-test')) {
    $taskProcess=Start-Process -FilePath $taskExecutable -ArgumentList $taskMode -WindowStyle Hidden -PassThru
    if(-not $taskProcess.WaitForExit(20000)){throw ('Portable timeout: '+$taskMode)}
    if($taskProcess.ExitCode -ne 0){throw ('Portable check failed: '+$taskMode)}
}
# Exercise normal startup recovery in the isolated copy, not user configuration.
Set-Content -LiteralPath $taskProfile -Value 'version 2'
Set-Content -LiteralPath (Join-Path $taskCopy 'config/settings.user.txt') -Value 'theme_mode invalid'
Add-Type -TypeDefinition @'
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class ResoniqPortableWindow {
    public delegate bool Callback(IntPtr window, IntPtr parameter);
    [DllImport("user32.dll")] static extern bool EnumWindows(Callback callback, IntPtr parameter);
    [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr window, out uint id);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] static extern int GetClassName(IntPtr window, StringBuilder name, int length);
    [DllImport("user32.dll")] static extern bool PostMessage(IntPtr window,uint message,IntPtr wParam,IntPtr lParam);
    public static bool Quit(uint process) {
        bool found=false;
        EnumWindows((window,parameter)=>{
            uint owner;GetWindowThreadProcessId(window,out owner);if(owner!=process)return true;
            var name=new StringBuilder(256);GetClassName(window,name,name.Capacity);
            if(name.ToString()!="Resoniq.Gui")return true;
            found=PostMessage(window,0x111,new IntPtr(304),IntPtr.Zero);return false;
        },IntPtr.Zero);return found;
    }
}
'@
$taskProcess=Start-Process -FilePath $taskExecutable -ArgumentList '--tray' -WindowStyle Hidden -PassThru
try {
    Start-Sleep -Seconds 3
    if($taskProcess.HasExited){throw 'Recovery startup failed'}
    $taskBackups=@(Get-ChildItem -LiteralPath (Join-Path $taskCopy 'config') -Filter '*.invalid-*')
    if($taskBackups.Count -ne 2){throw 'Damaged profile/preferences not preserved'}
    $taskTelemetry=Get-Content -LiteralPath (Join-Path $taskCopy 'build/gui-telemetry.log') -Raw
    if($taskTelemetry -notmatch 'startup_recovery=' -or $taskTelemetry -notmatch 'running=1 finished=0'){throw 'Recovered application has no active audio route'}
} finally {
    if(-not $taskProcess.HasExited) {
        if(-not [ResoniqPortableWindow]::Quit($taskProcess.Id)){throw 'Cannot stop isolated portable test'}
        if(-not $taskProcess.WaitForExit(10000)){throw 'Isolated portable test did not exit'}
    }
}
Write-Output ('PASS: portable GUI/audio, tamper rejection, corrupt profile/preferences recovery; evidence: '+$taskCopy)
