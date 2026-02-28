# 开发注意事项

> 记录开发过程中遇到的问题和解决方案，避免重复踩坑

## 工具使用规范

### 1. Edit 工具必须先 Read

**问题**：使用 Edit 工具前必须先 Read 文件，否则会报错 `File has not been read yet`

**正确做法**：
```
1. 先调用 Read 读取目标文件
2. 从 Read 输出中复制精确的字符串
3. 调用 Edit 进行替换
```

**错误示例**：
- 直接调用 Edit 而没有先 Read
- 手动输入 old_string 而不是从 Read 输出复制

### 2. 字符串匹配必须精确

**问题**：Edit 工具的 old_string 必须与文件内容完全匹配，包括：
- 空行数量
- 缩进（空格/Tab）
- 换行符

**正确做法**：
- 从 Read 工具输出直接复制文本
- 注意 Read 输出中的行号前缀（如 `    10→`）后面才是实际内容
- 保留原始的空行和缩进

### 3. Grep 结果需谨慎处理

**问题**：Grep 输出包含行号和文件路径前缀

**正确做法**：
- 使用 `-n` 参数获取行号
- 使用 `-A`/`-B`/`-C` 获取上下文
- 复制代码时去掉行号前缀

## UE C++ 开发规范

### 1. 新增类文件

**步骤**：
1. 在 `Source/Skelot/Public/` 创建 .h 文件
2. 在 `Source/Skelot/Private/` 创建 .cpp 文件
3. 确保包含正确的模块头文件
4. 编译测试

### 2. 头文件包含顺序

```cpp
// 1. 预编译头
#include "CoreMinimal.h"

// 2. 引擎头文件
#include "Engine/World.h"

// 3. 项目头文件
#include "SkelotWorldBase.h"

// 4. 生成的头文件（最后）
#include "SkelotWorld.generated.h"
```

### 3. UCLASS/USTRUCT 宏

- 必须在 `.generated.h` 包含之前定义
- BlueprintType 需要添加 `BlueprintType` 说明符
- 使用 `UENUM(BlueprintType)` 使枚举可在蓝图使用

## 常见编译错误

### 1. 链接错误 LNK2019

**原因**：函数声明但未实现

**解决**：检查 .cpp 文件中是否实现了所有声明的函数

### 2. 未定义的外部符号

**原因**：忘记添加模块导出宏

**解决**：确保类声明中有 `class MODULENAME_API ClassName`

### 3. 头文件循环依赖

**原因**：A 包含 B，B 包含 A

**解决**：使用前向声明 `class ClassName;` 替代 `#include`

## Git 提交规范

### 提交信息格式

```
<type>(<scope>): <subject>

<body>
```

### Type 类型

| 类型 | 说明 |
|------|------|
| feat | 新功能 |
| fix | Bug 修复 |
| refactor | 重构（不改变功能） |
| docs | 文档更新 |
| test | 测试相关 |
| chore | 构建/工具变更 |

### 示例

```
feat(Skelot): 实现空间哈希网格系统

- 新增 FSkelotSpatialGrid 类
- 添加球形范围查询 API
- 优化查询性能 O(n) -> O(1)
```

---

*最后更新: 2026-03-01*
