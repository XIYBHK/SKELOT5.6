@echo off
cd /d "%~dp0"
claude -p "请执行 Skelot 插件开发任务: 1.读取 docs/TASK_LIST.md 找优先级最高的待开始任务(P0>P1>P2,状态为空框) 2.参考 docs/参考插件_说明文档.md 实现 3.编译: pwsh -ExecutionPolicy Bypass -File BuildPlugin_CN.ps1 4.提交: git add -A 和 git commit 和 git push 5.更新任务状态为完成。确保编译通过，一次只执行一个任务。"
