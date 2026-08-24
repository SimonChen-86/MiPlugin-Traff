@echo off
setlocal
chcp 65001 >nul
title MiPlugin-Traff Build Console
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0build_release.ps1" %*
exit /b %ERRORLEVEL%
