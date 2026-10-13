@echo off
if exist "%~dp0bin\resoniq.exe" (
  start "" "%~dp0bin\resoniq.exe"
  exit /b 0
)
if not exist "%~dp0build\Release\resoniq.exe" (
  echo Compila primero la configuracion Release.
  pause
  exit /b 1
)
start "" "%~dp0build\Release\resoniq.exe"
