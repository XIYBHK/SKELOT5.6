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

## 空间网格优化调研结论

> 基于 2024-2026 年最新研究和 DetourCrowd 实践

### 已验证有效的方案

| 方案 | 效果 | 实现难度 | 是否采用 |
|------|------|----------|----------|
| **降低更新频率** | FrameStride=2 降 50% | 低 | ✅ 已实现 |
| **Cell Size = 2x 对象大小** | 最优查询效率 | 低 | ✅ 已实现 |

### 已验证无效/过度的方案

| 方案 | 问题 | 结论 |
|------|------|------|
| **Morton 编码** | 只对连续数组存储有效，`TMap` 无收益 | ❌ 不采用 |
| **分批更新** | 导致网格数据不一致，查询结果错误 | ❌ 不采用 |
| **增量更新** | 复杂度高，[Unity 经验表明](https://m.blog.csdn.net/gitblog_00325/article/details/153719621)变化累积后反而更慢 | ❌ 不采用 |
| **双缓冲** | 无异步查询需求时是内存浪费 | ⏸️ 暂不需要 |

### 分帧更新实现（已验证）

```cpp
// 基于 DetourCrowd 的简化方案
void RebuildIncremental(...)
{
    CurrentFrameIndex = (CurrentFrameIndex + 1) % FrameStride;

    // 只有当 CurrentFrameIndex == 0 时才重建
    if (CurrentFrameIndex == 0)
    {
        Rebuild(SOA, NumInstances);
    }
    // 否则跳过，使用上一帧数据
}
```

**优点**：
1. 简单实现，不会出错
2. 网格数据始终一致
3. 对群集 AI 场景足够（不需要每帧精确位置）

### 参考资料

- [DetourCrowd 分析](https://blog.csdn.net/gitblog_00648/article/details/154421533) - MAX_ITERS_PER_UPDATE 模式
- [空间哈希最佳实践](https://m.php.cn/faq/1927461.html) - Cell Size 设置
- [Unity Tilemap 经验](https://m.blog.csdn.net/gitblog_00325/article/details/153719621) - 增量更新 vs 完整重建

---

## 预研文档使用规范

### 重要原则

1. **优先查阅预研文档**：`docs/IMPLEMENTATION_CHALLENGES.md` 和 `docs/TECHNICAL_RESEARCH.md`
2. **批判性思考**：预研文档的建议需要验证，不是无条件执行
3. **网络调研验证**：对不确定的方案，先搜索验证再实现

### 预研文档评估流程

```
1. 阅读预研文档建议
2. 批判性分析：
   - 是否适用于当前场景？
   - 复杂度是否合理？
   - 是否有更好的替代方案？
3. 必要时进行网络调研
4. 做出决定并记录原因
```

---

*最后更新: 2026-03-01*
