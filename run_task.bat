@echo off
cd /d "%~dp0"
echo Starting Claude Code...
claude -p "Execute Skelot development task: 1. Read docs/TASK_LIST.md find highest priority pending task (P0 first) 2. Implement feature 3. Build: pwsh -ExecutionPolicy Bypass -File BuildPlugin_CN.ps1 4. Commit and push 5. Update task status. One task at a time, ensure build passes."
pause
