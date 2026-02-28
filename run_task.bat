@echo off
cd /d "%~dp0"
pwsh -ExecutionPolicy Bypass -File run_task.ps1
