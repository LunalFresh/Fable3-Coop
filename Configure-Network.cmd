@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Configure-Network.ps1" %*
pause
