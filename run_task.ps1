# Skelot 任务执行器
# 读取任务列表 → 执行 → 编译 → 提交

$Host.UI.RawUI.WindowTitle = "Skelot 任务执行器"

Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "         Skelot 插件开发 - 自动任务执行器" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "  [1] 执行下一个任务" -ForegroundColor Green
Write-Host "  [2] 执行指定任务" -ForegroundColor Green
Write-Host "  [3] 查看待处理任务" -ForegroundColor Green
Write-Host "  [4] 退出" -ForegroundColor Green
Write-Host ""

$choice = Read-Host "请选择"

$prompt = @"
请执行 Skelot 插件开发任务:

1. 读取 docs/TASK_LIST.md 找优先级最高的待开始任务 (P0>P1>P2, 状态为 ⬜)
2. 参考 docs/参考插件_说明文档.md 和 docs/TECHNICAL_RESEARCH.md 实现功能
3. 编译测试: pwsh -ExecutionPolicy Bypass -File BuildPlugin_CN.ps1
4. 提交: git add -A; git commit -m "feat: 描述"; git push
5. 更新任务状态为 ✅ 并提交

注意: 确保编译通过后再提交，一次只执行一个任务。
"@

if ($choice -eq "1") {
    Write-Host "正在启动 Claude Code..." -ForegroundColor Yellow
    & claude -p $prompt
}
elseif ($choice -eq "2") {
    $taskId = Read-Host "请输入任务编号"
    $prompt = @"
请执行 Skelot 插件开发任务 [$taskId]:

1. 在 docs/TASK_LIST.md 中找到任务 $taskId
2. 参考文档实现功能
3. 编译测试: pwsh -ExecutionPolicy Bypass -File BuildPlugin_CN.ps1
4. 提交: git add -A; git commit -m "feat: 描述"; git push
5. 更新任务状态为 ✅

确保编译通过后再提交。
"@
    Write-Host "正在启动 Claude Code..." -ForegroundColor Yellow
    & claude -p $prompt
}
elseif ($choice -eq "3") {
    Write-Host ""
    Write-Host "待处理任务:" -ForegroundColor Yellow
    Select-String -Path "docs\TASK_LIST.md" -Pattern "⬜" | Select-Object -First 15 | ForEach-Object {
        Write-Host $_.Line.Trim() -ForegroundColor White
    }
    Write-Host ""
    Read-Host "按回车键继续"
    & $PSCommandPath
}
elseif ($choice -eq "4") {
    exit
}
else {
    Write-Host "无效选择" -ForegroundColor Red
}
