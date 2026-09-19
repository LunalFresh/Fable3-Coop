@echo off
setlocal

set "GAME_ROOT=%~dp0"
set "LAUNCHER=%GAME_ROOT%FableLauncher.exe"
set "XLLN_CONFIG=%GAME_ROOT%XLiveLessNess\xlln-config-1.ini"
set "FABLE_MODULE=%GAME_ROOT%XLiveLessNess\modules\xlln-fable-3.dll"

if not exist "%LAUNCHER%" (
    echo ERROR: FableLauncher.exe was not found beside this file.
    echo Keep Start-Fable3-LAN.bat in the Fable III game folder.
    pause
    exit /b 1
)

if not exist "%XLLN_CONFIG%" (
    echo ERROR: The automatic XLLN configuration is missing:
    echo %XLLN_CONFIG%
    pause
    exit /b 1
)

if not exist "%FABLE_MODULE%" (
    echo ERROR: The Fable III XLLN module is missing:
    echo %FABLE_MODULE%
    pause
    exit /b 1
)

if /I "%~1"=="--check" (
    echo Fable III LAN launcher check passed.
    exit /b 0
)

tasklist /FI "IMAGENAME eq Fable3.exe" /NH 2>nul | find /I "Fable3.exe" >nul
if not errorlevel 1 (
    echo Fable III is already running.
    pause
    exit /b 1
)

echo Opening the official Fable III launcher with the LAN configuration ready...
start "" /D "%GAME_ROOT%" "%LAUNCHER%"
exit /b 0
