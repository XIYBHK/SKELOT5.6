# UE 插件快速编译检查脚本
# 编译代码检查错误，输出到临时目录（编译后自动清理）
# 用法: pwsh -ExecutionPolicy Bypass -File QuickCompile.ps1

param()

# 设置 UTF-8 编码
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$OutputEncoding = [System.Text.Encoding]::UTF8

# 获取脚本所在目录（插件根目录）
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

# 自动查找 .uplugin 文件
$PluginFile = Get-ChildItem -Path $ScriptDir -Filter "*.uplugin" -File | Select-Object -First 1

if (-not $PluginFile) {
    Write-Host "错误: 未找到 .uplugin 文件" -ForegroundColor Red
    exit 1
}

$PluginName = $PluginFile.BaseName

# UE 路径
$UEPath = "F:\Epic Games\UE_5.6"
$UATPath = "$UEPath\Engine\Build\BatchFiles\RunUAT.bat"

# 临时输出目录（编译后删除）
$TempOutput = Join-Path $env:TEMP "Skelot_QuickCompile_$([guid]::NewGuid().ToString())"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  快速编译检查: $PluginName" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# 检查 UAT
if (-not (Test-Path $UATPath)) {
    Write-Host "错误: 未找到 RunUAT" -ForegroundColor Red
    Write-Host "路径: $UATPath" -ForegroundColor Yellow
    exit 1
}

Write-Host "[编译中...]" -ForegroundColor Yellow
Write-Host "(编译 Editor + Game 目标，输出到临时目录)" -ForegroundColor DarkGray
Write-Host ""

# 使用 BuildPlugin 但输出到临时目录
$psi = New-Object System.Diagnostics.ProcessStartInfo
$psi.FileName = $UATPath
$psi.Arguments = "BuildPlugin -Plugin=`"$($PluginFile.FullName)`" -Package=`"$TempOutput`" -TargetPlatforms=Win64 -VS2022"
$psi.RedirectStandardOutput = $true
$psi.RedirectStandardError = $true
$psi.UseShellExecute = $false
$psi.CreateNoWindow = $true

$process = New-Object System.Diagnostics.Process
$process.StartInfo = $psi
$process.Start() | Out-Null

# 实时读取输出，过滤掉一些不重要的信息
$stdout = $process.StandardOutput
while ($null -ne ($line = $stdout.ReadLine())) {
    # 过滤一些噪音，保留关键信息
    if ($line -match "error|Error:|错误|FAILED|Failed") {
        Write-Host $line -ForegroundColor Red
    } elseif ($line -match "warning|Warning:") {
        Write-Host $line -ForegroundColor Yellow
    } elseif ($line -match "Building|------ Building|Linking") {
        Write-Host "  $line" -ForegroundColor DarkGray
    } elseif ($line -match "Total execution time|Result:") {
        Write-Host $line
    }
}

$errorOutput = $process.StandardError.ReadToEnd()
if ($errorOutput) {
    Write-Host $errorOutput -ForegroundColor Red
}

$process.WaitForExit()
$exitCode = $process.ExitCode

# 清理临时目录
if (Test-Path $TempOutput) {
    try {
        Remove-Item -Path $TempOutput -Recurse -Force -ErrorAction SilentlyContinue
    } catch {}
}

Write-Host ""
if ($exitCode -eq 0) {
    Write-Host "========================================" -ForegroundColor Green
    Write-Host "  编译通过！" -ForegroundColor Green
    Write-Host "========================================" -ForegroundColor Green
} else {
    Write-Host "========================================" -ForegroundColor Red
    Write-Host "  编译失败！" -ForegroundColor Red
    Write-Host "========================================" -ForegroundColor Red
}

exit $exitCode
