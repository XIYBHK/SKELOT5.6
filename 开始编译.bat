@echo off
chcp 65001 >nul
echo 正在启动插件编译工具...
echo.
pwsh -ExecutionPolicy Bypass -NoExit -File "%~dp0BuildPlugin_CN.ps1"
if errorlevel 1 (
    echo.
    echo 启动失败，请确保已安装 PowerShell 7
    echo 或使用右键 - 使用 PowerShell 运行 BuildPlugin_CN.ps1
    pause
)
