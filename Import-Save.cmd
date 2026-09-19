@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Import-Save.ps1" %*
pause
