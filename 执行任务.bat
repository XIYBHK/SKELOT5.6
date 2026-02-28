@echo off
chcp 65001 >nul
title Skelot 任务执行器

echo ╔════════════════════════════════════════════════════════════╗
echo ║           Skelot 插件开发 - 自动任务执行器                  ║
echo ╠════════════════════════════════════════════════════════════╣
echo ║  [1] 执行下一个任务 (自动选择优先级最高的)                   ║
echo ║  [2] 执行指定任务 (输入任务编号)                            ║
echo ║  [3] 查看任务列表                                          ║
echo ║  [4] 退出                                                  ║
echo ╚════════════════════════════════════════════════════════════╝
echo.

cd /d "%~dp0"

set /p choice="请选择操作 [1-4]: "

if "%choice%"=="1" goto EXECUTE_NEXT
if "%choice%"=="2" goto EXECUTE_SPECIFIC
if "%choice%"=="3" goto VIEW_TASKS
if "%choice%"=="4" goto END

:EXECUTE_NEXT
echo.
echo 正在启动 Claude Code 执行下一个任务...
echo.
claude -p "执行 Skelot 开发任务: 1.读 docs/TASK_LIST.md 找最高优先级待开始任务(P0>P1>P2,⬜状态) 2.参考 docs/参考插件_说明文档.md 实现 3.编译: pwsh -ExecutionPolicy Bypass -File BuildPlugin_CN.ps1 4.提交: git add -A && git commit -m \"feat: 描述\" && git push 5.更新任务状态为✅。一次一个任务，确保编译通过。"
goto END

:EXECUTE_SPECIFIC
set /p task_id="请输入任务编号 (如 1.1.1): "
echo.
echo 正在启动 Claude Code 执行任务 [%task_id%]...
echo.
claude -p "执行 Skelot 开发任务 [%task_id%]: 1.读 docs/TASK_LIST.md 找任务 %task_id% 2.参考 docs/参考插件_说明文档.md 实现 3.编译: pwsh -ExecutionPolicy Bypass -File BuildPlugin_CN.ps1 4.提交: git add -A && git commit -m \"feat: 描述\" && git push 5.更新任务状态为✅。确保编译通过。"
goto END

:VIEW_TASKS
echo.
echo 任务列表:
echo ----------------------------------------
type docs\TASK_LIST.md | findstr /C:"P0" /C:"P1" /C:"P2" | findstr /C:"⬜"
echo ----------------------------------------
echo.
pause
cls
goto :eof

:END
