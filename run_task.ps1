# Skelot 任务执行器 - 直接启动 Claude Code
cd $PSScriptRoot

$prompt = @"
请执行 Skelot 插件开发任务:

1. 读取 docs/TASK_LIST.md 找优先级最高的待开始任务 (P0>P1>P2, 状态为 ⬜)
2. 参考 docs/参考插件_说明文档.md 和 docs/TECHNICAL_RESEARCH.md 实现功能
3. 编译测试: pwsh -ExecutionPolicy Bypass -File BuildPlugin_CN.ps1
4. 提交: git add -A; git commit -m "feat: 描述"; git push
5. 更新任务状态为 ✅ 并提交

注意: 确保编译通过后再提交，一次只执行一个任务。
"@

claude -p $prompt
