# -*- coding: utf-8 -*-
"""
Skelot 插件开发 - 自动任务执行器
读取任务列表 → 拉取任务 → 执行 → 编译 → 提交
"""

import os
import subprocess
import sys

# 切换到脚本所在目录
os.chdir(os.path.dirname(os.path.abspath(__file__)))

def clear_screen():
    os.system('cls' if os.name == 'nt' else 'clear')

def print_header():
    print("=" * 60)
    print("         Skelot 插件开发 - 自动任务执行器")
    print("=" * 60)
    print()

def print_menu():
    print("  [1] 执行下一个任务 (自动选择优先级最高的)")
    print("  [2] 执行指定任务 (输入任务编号)")
    print("  [3] 查看待处理任务")
    print("  [4] 查看任务列表文件")
    print("  [5] 退出")
    print()

def read_task_list():
    """读取任务列表文件"""
    task_file = "docs/TASK_LIST.md"
    if not os.path.exists(task_file):
        print(f"错误: 找不到任务文件 {task_file}")
        return None
    with open(task_file, 'r', encoding='utf-8') as f:
        return f.read()

def find_pending_tasks(content):
    """查找待处理的任务"""
    import re
    lines = content.split('\n')
    pending_tasks = []

    for i, line in enumerate(lines):
        # 查找包含 ⬜ 的任务行
        if '⬜' in line:
            # 提取优先级
            priority = None
            if '| P0 |' in line:
                priority = 'P0'
            elif '| P1 |' in line:
                priority = 'P1'
            elif '| P2 |' in line:
                priority = 'P2'

            # 提取任务描述
            parts = line.split('|')
            if len(parts) >= 3:
                task_desc = parts[2].strip() if len(parts) > 2 else "未知任务"
                task_id = parts[1].strip() if len(parts) > 1 else "?"
                pending_tasks.append({
                    'id': task_id,
                    'desc': task_desc,
                    'priority': priority or 'P?',
                    'line': i + 1,
                    'raw': line
                })

    return pending_tasks

def view_pending_tasks():
    """查看待处理任务"""
    content = read_task_list()
    if not content:
        return

    tasks = find_pending_tasks(content)

    # 按优先级排序
    priority_order = {'P0': 0, 'P1': 1, 'P2': 2, 'P?': 3}
    tasks.sort(key=lambda x: priority_order.get(x['priority'], 3))

    print("-" * 60)
    print("待处理任务列表 (按优先级排序):")
    print("-" * 60)

    for task in tasks[:20]:  # 只显示前20个
        print(f"  [{task['priority']}] {task['desc'][:40]}")

    print("-" * 60)
    print(f"共 {len(tasks)} 个待处理任务")
    print()

def execute_next_task():
    """执行下一个优先级最高的任务"""
    content = read_task_list()
    if not content:
        return

    tasks = find_pending_tasks(content)

    if not tasks:
        print("没有待处理的任务!")
        return

    # 按优先级排序，选择第一个
    priority_order = {'P0': 0, 'P1': 1, 'P2': 2, 'P?': 3}
    tasks.sort(key=lambda x: priority_order.get(x['priority'], 3))
    next_task = tasks[0]

    print(f"即将执行任务: [{next_task['priority']}] {next_task['desc']}")
    print()

    prompt = f"""请执行 Skelot 插件开发任务。

任务信息:
- 优先级: {next_task['priority']}
- 描述: {next_task['desc']}

执行流程:
1. 阅读 docs/TASK_LIST.md 了解任务详情
2. 参考 docs/参考插件_说明文档.md 了解功能需求
3. 参考 docs/TECHNICAL_RESEARCH.md 中的代码模板
4. 实现功能代码
5. 编译测试: pwsh -ExecutionPolicy Bypass -File BuildPlugin_CN.ps1
6. 如果编译成功:
   - git add -A
   - git commit -m "feat: {next_task['desc']}"
   - git push
7. 更新 docs/TASK_LIST.md 中该任务状态从 ⬜ 改为 ✅
8. 提交状态更新

注意: 确保编译通过后再提交，一次只执行这一个任务。
"""

    run_claude(prompt)

def execute_specific_task():
    """执行指定任务"""
    task_id = input("请输入任务编号 (如 1.1.1): ").strip()

    if not task_id:
        print("任务编号不能为空")
        return

    content = read_task_list()
    if not content:
        return

    # 查找指定任务
    lines = content.split('\n')
    task_line = None
    task_desc = ""

    for i, line in enumerate(lines):
        if task_id in line and '⬜' in line:
            task_line = i + 1
            parts = line.split('|')
            if len(parts) >= 3:
                task_desc = parts[2].strip()
            break

    if not task_line:
        print(f"未找到任务编号 {task_id} 或任务已完成")
        return

    print(f"找到任务: {task_desc}")
    print()

    prompt = f"""请执行 Skelot 插件开发任务。

任务信息:
- 编号: {task_id}
- 描述: {task_desc}

执行流程:
1. 阅读 docs/TASK_LIST.md 第 {task_line} 行附近的任务详情
2. 参考 docs/参考插件_说明文档.md 了解功能需求
3. 参考 docs/TECHNICAL_RESEARCH.md 中的代码模板
4. 实现功能代码
5. 编译测试: pwsh -ExecutionPolicy Bypass -File BuildPlugin_CN.ps1
6. 如果编译成功:
   - git add -A
   - git commit -m "feat: {task_desc}"
   - git push
7. 更新 docs/TASK_LIST.md 中该任务状态从 ⬜ 改为 ✅
8. 提交状态更新

注意: 确保编译通过后再提交。
"""

    run_claude(prompt)

def run_claude(prompt):
    """启动 Claude Code"""
    print("正在启动 Claude Code...")
    print("-" * 60)
    print()

    try:
        # 使用 subprocess 运行 claude 命令
        result = subprocess.run(
            ['claude', '-p', prompt],
            cwd=os.getcwd()
        )
        print()
        print("-" * 60)
        print("任务执行完毕")
    except FileNotFoundError:
        print("错误: 找不到 claude 命令，请确保 Claude Code CLI 已安装")
        print("安装方法: npm install -g @anthropic-ai/claude-code")
    except Exception as e:
        print(f"执行出错: {e}")

def open_task_file():
    """打开任务列表文件"""
    task_file = "docs/TASK_LIST.md"
    if os.path.exists(task_file):
        if os.name == 'nt':
            os.system(f'notepad {task_file}')
        else:
            os.system(f'nano {task_file}')
    else:
        print(f"找不到文件: {task_file}")

def main():
    while True:
        clear_screen()
        print_header()
        print_menu()

        choice = input("请选择操作 [1-5]: ").strip()
        print()

        if choice == '1':
            execute_next_task()
        elif choice == '2':
            execute_specific_task()
        elif choice == '3':
            view_pending_tasks()
            input("\n按回车键继续...")
        elif choice == '4':
            open_task_file()
        elif choice == '5':
            print("再见!")
            break
        else:
            print("无效选择，请重新输入")
            input("按回车键继续...")

if __name__ == '__main__':
    main()
