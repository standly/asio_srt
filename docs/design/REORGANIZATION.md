# BCAST 目录重组说明

## 重组概述

为了更好地组织代码和文档，我们对 `bcast` 项目进行了全面的目录重组。

## 重组前后对比

### 重组前（src/bcast/ 下）

```
src/bcast/
├── async_queue.hpp              # 核心文件
├── dispatcher.hpp               # 核心文件
├── CMakeLists.txt               # 构建文件
├── README.md                    # 说明文档
│
├── example.cpp                  # 示例 1
├── advanced_example.cpp         # 示例 2
├── coroutine_example.cpp        # 示例 3
├── real_world_example.cpp       # 示例 4
├── timeout_example.cpp          # 示例 5
├── batch_example.cpp            # 示例 6
├── benchmark.cpp                # 示例 7
│
├── 快速开始.md                  # 文档 1
├── 使用指南.md                  # 文档 2
├── 协程API指南.md               # 文档 3
├── API对比.md                   # 文档 4
├── 批量操作指南.md              # 文档 5
├── 超时功能指南.md              # 文档 6
├── 项目总结.md                  # 文档 7
└── 代码简化说明.md              # 文档 8
```

**问题：**
- 核心代码、示例、文档混在一起
- 目录结构混乱，难以导航
- 文档重复内容多
- 不符合常规项目结构

### 重组后

```
src/bcast/                      # 核心源代码
├── async_queue.hpp             # 异步队列
├── dispatcher.hpp              # 消息分发器
├── CMakeLists.txt              # 构建配置
└── README.md                   # 简要说明

examples/bcast/                 # 示例代码
├── CMakeLists.txt
├── example.cpp                 # 回调方式示例
├── advanced_example.cpp        # 高级模式
├── coroutine_example.cpp       # 协程基础
├── real_world_example.cpp      # 实战场景
├── timeout_example.cpp         # 超时功能
├── batch_example.cpp           # 批量操作
└── benchmark.cpp               # 性能测试

docs/bcast/                     # 文档
├── README.md                   # 文档导航
├── BCAST_GUIDE.md              # 完整使用指南（合并）
├── BATCH_OPERATIONS.md         # 批量操作详解
├── TIMEOUT_FEATURES.md         # 超时功能详解
└── REORGANIZATION.md           # 本文档
```

**优势：**
- ✅ 清晰的三层结构：核心代码、示例、文档
- ✅ 符合标准项目结构
- ✅ 易于导航和维护
- ✅ 文档整合，减少重复

## 详细变化

### 1. 核心代码 (src/bcast/)

**保留文件：**
- `async_queue.hpp` - 不变
- `dispatcher.hpp` - 不变
- `CMakeLists.txt` - 简化，移除示例构建
- `README.md` - 重写，简洁明了

**变化：**
- CMakeLists.txt 只负责定义 bcast 库
- README.md 指向详细文档

### 2. 示例代码 (examples/bcast/)

**移动的文件：**
- `example.cpp` - 从 src/bcast/ 移动
- `advanced_example.cpp` - 从 src/bcast/ 移动
- `coroutine_example.cpp` - 从 src/bcast/ 移动
- `real_world_example.cpp` - 从 src/bcast/ 移动
- `timeout_example.cpp` - 从 src/bcast/ 移动
- `batch_example.cpp` - 从 src/bcast/ 移动
- `benchmark.cpp` - 从 src/bcast/ 移动

**新增文件：**
- `CMakeLists.txt` - 示例的构建配置

**集成：**
- 在 `examples/CMakeLists.txt` 中添加了 `add_subdirectory(bcast)`

### 3. 文档 (docs/bcast/)

**新增文件：**
- `README.md` - 文档导航
- `BCAST_GUIDE.md` - 完整使用指南（合并了4个文档）
- `REORGANIZATION.md` - 本文档

**移动和重命名：**
- `批量操作指南.md` → `BATCH_OPERATIONS.md`
- `超时功能指南.md` → `TIMEOUT_FEATURES.md`

**合并的文档：**
以下文档已合并到 `BCAST_GUIDE.md`：
- `快速开始.md` → 合并到"快速开始"章节
- `使用指南.md` → 合并到"API 参考"章节
- `协程API指南.md` → 合并到"协程接口"章节
- `API对比.md` → 部分内容合并

**删除的文档：**
- `项目总结.md` - 信息已过时，删除
- `代码简化说明.md` - 开发笔记，不需要保留

## 文档合并说明

### BCAST_GUIDE.md 内容结构

原来的 4 个文档合并为一个完整的指南：

1. **快速开始**（来自 快速开始.md）
   - 最小示例
   - 常见场景

2. **核心概念**（来自 使用指南.md）
   - Strand
   - async_queue
   - dispatcher

3. **API 参考**（来自 使用指南.md）
   - dispatcher API
   - async_queue API

4. **协程接口**（来自 协程API指南.md）
   - 基本读取
   - 批量读取
   - 超时读取
   - 错误处理

5. **高级特性**（整合）
   - 批量操作概述
   - 超时机制概述
   - 多线程环境

6. **最佳实践**（来自 使用指南.md + 协程API指南.md）
   - io_context 管理
   - 消息优化
   - 生命周期管理

7. **常见问题**（来自 使用指南.md）

**优势：**
- 一个文档包含完整信息
- 逻辑结构清晰
- 易于学习和查阅
- 减少文档间跳转

## 文件路径映射

### 示例文件

| 旧路径 | 新路径 |
|--------|--------|
| `src/bcast/example.cpp` | `examples/bcast/example.cpp` |
| `src/bcast/coroutine_example.cpp` | `examples/bcast/coroutine_example.cpp` |
| `src/bcast/timeout_example.cpp` | `examples/bcast/timeout_example.cpp` |
| `src/bcast/batch_example.cpp` | `examples/bcast/batch_example.cpp` |
| ... | ... |

### 文档文件

| 旧文件 | 新文件 | 说明 |
|--------|--------|------|
| `src/bcast/快速开始.md` | `docs/bcast/BCAST_GUIDE.md` | 合并 |
| `src/bcast/使用指南.md` | `docs/bcast/BCAST_GUIDE.md` | 合并 |
| `src/bcast/协程API指南.md` | `docs/bcast/BCAST_GUIDE.md` | 合并 |
| `src/bcast/API对比.md` | - | 删除（内容已整合） |
| `src/bcast/批量操作指南.md` | `docs/bcast/BATCH_OPERATIONS.md` | 移动+重命名 |
| `src/bcast/超时功能指南.md` | `docs/bcast/TIMEOUT_FEATURES.md` | 移动+重命名 |
| `src/bcast/项目总结.md` | - | 删除 |
| `src/bcast/代码简化说明.md` | - | 删除 |

## 使用指南

### 对于开发者

**查看核心代码：**
```bash
cd src/bcast
ls *.hpp
```

**运行示例：**
```bash
cd examples/bcast
g++ -std=c++20 -fcoroutines coroutine_example.cpp -lpthread -o example
./example
```

**阅读文档：**
```bash
cd docs/bcast
# 从 README.md 开始
# 然后阅读 BCAST_GUIDE.md
```

### 对于用户

**快速开始：**
1. 阅读 `src/bcast/README.md` 了解基本概念
2. 阅读 `docs/bcast/BCAST_GUIDE.md` 的"快速开始"部分
3. 运行 `examples/bcast/coroutine_example.cpp`

**深入学习：**
1. 完整阅读 `docs/bcast/BCAST_GUIDE.md`
2. 学习特性文档：`BATCH_OPERATIONS.md`, `TIMEOUT_FEATURES.md`
3. 研究所有示例代码

## 构建系统更新

### src/bcast/CMakeLists.txt

```cmake
# 仅定义 bcast 库
add_library(bcast INTERFACE)
target_include_directories(bcast INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/..)
```

### examples/bcast/CMakeLists.txt

```cmake
# 构建所有示例
add_executable(bcast_coroutine_example coroutine_example.cpp)
# ...
```

### examples/CMakeLists.txt

```cmake
# 添加了
add_subdirectory(bcast)
```

## 迁移指南

### 如果你之前引用了文档

| 旧链接 | 新链接 |
|--------|--------|
| `src/bcast/快速开始.md` | `docs/bcast/BCAST_GUIDE.md#快速开始` |
| `src/bcast/使用指南.md` | `docs/bcast/BCAST_GUIDE.md` |
| `src/bcast/协程API指南.md` | `docs/bcast/BCAST_GUIDE.md#协程接口` |
| `src/bcast/批量操作指南.md` | `docs/bcast/BATCH_OPERATIONS.md` |
| `src/bcast/超时功能指南.md` | `docs/bcast/TIMEOUT_FEATURES.md` |

### 如果你之前引用了示例

| 旧路径 | 新路径 |
|--------|--------|
| `src/bcast/coroutine_example.cpp` | `examples/bcast/coroutine_example.cpp` |
| `src/bcast/batch_example.cpp` | `examples/bcast/batch_example.cpp` |
| ... | ... |

## 总结

这次重组带来的改进：

1. ✅ **清晰的结构** - 核心代码、示例、文档分离
2. ✅ **符合标准** - 遵循常规项目结构
3. ✅ **易于维护** - 每个目录职责单一
4. ✅ **文档整合** - 减少重复，易于学习
5. ✅ **更好的导航** - 明确的文件组织

重组后的项目更加专业、易用、可维护！

## 反馈

如果你对这次重组有任何建议或发现问题，欢迎反馈！

