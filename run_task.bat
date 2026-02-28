@echo off
cd /d "%~dp0"
claude -p "请执行 Skelot 插件开发任务: 1.读取 docs/TASK_LIST.md 找优先级最高的待开始任务(优先级P0然后P1然后P2,状态为待开始) 2.参考 docs/参考插件_说明文档.md 实现 3.编译: pwsh -ExecutionPolicy Bypass -File BuildPlugin_CN.ps1 4.提交: git add -A 然后 git commit 然后 git push 5.更新任务状态为已完成。确保编译通过，一次只执行一个任务。"
