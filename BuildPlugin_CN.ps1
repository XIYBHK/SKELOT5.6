# UE 插件编译脚本 - 通用版本
# 将此脚本放在插件根目录（与 .uplugin 文件同级）下运行
param()

# 设置 UTF-8 编码
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$OutputEncoding = [System.Text.Encoding]::UTF8

# 获取脚本所在目录（插件根目录）
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

# 自动查找 .uplugin 文件
$PluginFile = Get-ChildItem -Path $ScriptDir -Filter "*.uplugin" -File | Select-Object -First 1

if (-not $PluginFile) {
    Write-Host "========================================" -ForegroundColor Red
    Write-Host "           错误！" -ForegroundColor Red
    Write-Host "========================================" -ForegroundColor Red
    Write-Host ""
    Write-Host "未找到 .uplugin 文件！" -ForegroundColor Red
    Write-Host ""
    Write-Host "请将此脚本放在插件根目录下（与 .uplugin 文件同级）" -ForegroundColor Yellow
    Write-Host ""
    Read-Host "按 Enter 键退出"
    exit 1
}

$PluginName = $PluginFile.BaseName
$PluginPath = $PluginFile.FullName
$OutputPath = Join-Path (Split-Path -Parent $ScriptDir) "CompiledPlugin"

# UE 和 VS 路径（可根据需要修改）
$UEPath = "F:\Epic Games\UE_5.6"
$UATPath = "$UEPath\Engine\Build\BatchFiles\RunUAT.bat"
$VS2022Path = "E:\VisualStudio\2022\Common7\IDE"

function Show-Header {
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host "  UE 插件编译工具" -ForegroundColor Cyan
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host ""
}

function Show-Config {
    Write-Host "[插件信息]" -ForegroundColor Yellow
    Write-Host "插件名称: $PluginName"
    Write-Host "插件路径: $PluginPath"
    Write-Host ""
    Write-Host "[环境配置]" -ForegroundColor Yellow
    Write-Host "UE 5.6 路径: $UEPath"
    Write-Host "VS 2022 路径: $VS2022Path"
    Write-Host ""
    Write-Host "[操作步骤]" -ForegroundColor Yellow
    Write-Host "1. 检查 UE 5.6 安装"
    Write-Host "2. 检查插件文件"
    Write-Host "3. 编译插件 (Win64 平台)"
    Write-Host "4. 输出到 CompiledPlugin 目录"
    Write-Host ""
    Read-Host "按 Enter 键开始编译"
}

function Test-UE {
    Write-Host "[1/4] 检查 UE 5.6..." -ForegroundColor Yellow
    if (Test-Path $UATPath) {
        Write-Host "  [成功] UE 5.6 已找到" -ForegroundColor Green
        return $true
    } else {
        Write-Host "  [错误] 未找到 UE 5.6" -ForegroundColor Red
        Write-Host "  请确认 UE 5.6 安装在: $UEPath" -ForegroundColor Yellow
        return $false
    }
}

function Test-Plugin {
    Write-Host ""
    Write-Host "[2/4] 检查插件文件..." -ForegroundColor Yellow
    if (Test-Path $PluginPath) {
        Write-Host "  [成功] 插件文件已找到" -ForegroundColor Green
        Write-Host "  插件: $PluginName"
        return $true
    } else {
        Write-Host "  [错误] 未找到插件文件" -ForegroundColor Red
        return $false
    }
}

function Clear-Output {
    Write-Host ""
    Write-Host "[3/4] 准备输出目录..." -ForegroundColor Yellow
    if (Test-Path $OutputPath) {
        Remove-Item -Path $OutputPath -Recurse -Force
        Write-Host "  已清理旧编译结果"
    }
    New-Item -ItemType Directory -Path $OutputPath -Force | Out-Null
    Write-Host "  [成功] 输出目录已准备"
    Write-Host "  输出: $OutputPath"
}

function Start-Build {
    Write-Host ""
    Write-Host "[4/4] 编译插件 (预计 2-5 分钟)..." -ForegroundColor Yellow
    Write-Host "  正在编译，实时显示日志..."
    Write-Host "  (按 Ctrl+C 可取消)"
    Write-Host ""
    Write-Host "----------------------------------------" -ForegroundColor DarkGray
    
    # 使用 Start-Process 实时显示输出
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $UATPath
    $psi.Arguments = "BuildPlugin -Plugin=`"$PluginPath`" -Package=`"$OutputPath`" -TargetPlatforms=Win64 -VS2022"
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.UseShellExecute = $false
    $psi.CreateNoWindow = $true
    
    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $psi
    $process.Start() | Out-Null
    
    # 实时读取标准输出
    $stdout = $process.StandardOutput
    while ($null -ne ($line = $stdout.ReadLine())) {
        Write-Host $line
    }
    
    # 读取错误输出
    $stderr = $process.StandardError
    $errorOutput = $stderr.ReadToEnd()
    if ($errorOutput) {
        Write-Host ""
        Write-Host "[错误输出]:" -ForegroundColor Red
        Write-Host $errorOutput -ForegroundColor Red
    }
    
    $process.WaitForExit()
    
    Write-Host "----------------------------------------" -ForegroundColor DarkGray
    return $process.ExitCode
}

function Show-Success {
    Write-Host ""
    Write-Host "========================================" -ForegroundColor Green
    Write-Host "           编译成功！" -ForegroundColor Green
    Write-Host "========================================" -ForegroundColor Green
    Write-Host ""
    Write-Host "插件名称: $PluginName"
    Write-Host "输出目录: $OutputPath"
    Write-Host ""
    Write-Host "[使用说明]" -ForegroundColor Yellow
    Write-Host "将 CompiledPlugin 文件夹复制到你的 UE 项目 Plugins 目录下"
}

function Show-Failure {
    Write-Host ""
    Write-Host "========================================" -ForegroundColor Red
    Write-Host "           编译失败！" -ForegroundColor Red
    Write-Host "========================================" -ForegroundColor Red
    Write-Host ""
    Write-Host "[解决方案]" -ForegroundColor Yellow
    Write-Host "请打开 Visual Studio Installer，安装以下组件："
    Write-Host "- 工作负载: 使用 C++ 的游戏开发"
    Write-Host "- 组件: MSVC v143 - VS 2022 C++ x64/x86 生成工具"
    Write-Host "- 组件: Windows 10/11 SDK"
    Write-Host "- 组件: .NET 6.0/7.0/8.0 SDK"
}

# 主流程
try {
    Show-Header
    Show-Config

    if (-not (Test-UE)) { 
        Read-Host "按 Enter 键退出"
        exit 1 
    }
    if (-not (Test-Plugin)) { 
        Read-Host "按 Enter 键退出"
        exit 1 
    }

    Clear-Output
    $ExitCode = Start-Build

    if ($ExitCode -eq 0) {
        Show-Success
    } else {
        Show-Failure
    }

    Write-Host ""
    Read-Host "按 Enter 键退出"

} catch {
    Write-Host "发生错误: $_" -ForegroundColor Red
    Read-Host "按 Enter 键退出"
}
