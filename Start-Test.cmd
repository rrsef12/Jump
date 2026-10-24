@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0dev.ps1" %*
if errorlevel 1 (
    echo.
    echo Test launch failed. See the error above.
    pause
    exit /b 1
)
