@echo off
setlocal

set "SCRIPT_DIR=%~dp0"
set "SERVER_EXE=%SCRIPT_DIR%Binaries\Win64\PUBG_HotModeServer.exe"

if not exist "%SERVER_EXE%" (
    echo ERROR: Dedicated server executable not found:
    echo   %SERVER_EXE%
    exit /b 1
)

start "" "%SERVER_EXE%" -log -port=7777
exit /b 0
