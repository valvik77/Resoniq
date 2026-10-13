[CmdletBinding()]
param([string]$Directory=[Environment]::GetFolderPath('Desktop'))
$ErrorActionPreference='Stop'
$taskRoot=Split-Path $PSScriptRoot -Parent
$taskExecutable=Join-Path $taskRoot 'bin/resoniq.exe'
if(-not (Test-Path -LiteralPath $taskExecutable)){throw 'Ejecuta este script desde el paquete portable extraido'}
$taskShortcutPath=Join-Path $Directory 'RESONIQ.lnk'
if(Test-Path -LiteralPath $taskShortcutPath){throw 'Ya existe RESONIQ.lnk; conserva el acceso anterior o elige otra carpeta'}
$taskShell=New-Object -ComObject WScript.Shell
$taskShortcut=$taskShell.CreateShortcut($taskShortcutPath)
$taskShortcut.TargetPath=$taskExecutable
$taskShortcut.WorkingDirectory=$taskRoot
$taskShortcut.Description='RESONIQ - audio del sistema'
$taskShortcut.Save()
Write-Output $taskShortcutPath
