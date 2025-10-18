# BCAST 文档目录

## 文档列表

### 主要文档

- **[BCAST_GUIDE.md](BCAST_GUIDE.md)** - 完整使用指南
  - 快速开始
  - 核心概念
  - API 参考
  - 协程接口
  - 高级特性
  - 最佳实践
  - 常见问题

### 特性文档

- **[BATCH_OPERATIONS.md](BATCH_OPERATIONS.md)** - 批量操作详解
  - 批量 push/publish API
  - 性能对比（10-100倍加速）
  - 使用场景
  - 最佳实践

- **[TIMEOUT_FEATURES.md](TIMEOUT_FEATURES.md)** - 超时功能详解
  - 超时读取 API
  - 实战场景（心跳检测、重试机制等）
  - 性能考虑
  - 错误处理

## 源代码

- **核心代码**：`src/bcast/`
  - `async_queue.hpp` - 异步消息队列
  - `dispatcher.hpp` - 消息分发器
  - `CMakeLists.txt` - 构建配置
  - `README.md` - 简要说明

## 示例代码

- **示例目录**：`examples/bcast/`
  - `coroutine_example.cpp` - 协程基础示例
  - `timeout_example.cpp` - 超时功能示例
  - `batch_example.cpp` - 批量操作示例
  - `real_world_example.cpp` - 实战场景
  - `example.cpp` - 回调方式示例
  - `advanced_example.cpp` - 高级模式
  - `benchmark.cpp` - 性能测试
  - `CMakeLists.txt` - 构建配置

## 快速导航

### 我是新手
1. 阅读 `src/bcast/README.md` 了解基本概念
2. 阅读 [BCAST_GUIDE.md](BCAST_GUIDE.md) 的"快速开始"部分
3. 运行 `examples/bcast/coroutine_example.cpp`

### 我想了解特定功能
- **批量操作** → [BATCH_OPERATIONS.md](BATCH_OPERATIONS.md)
- **超时机制** → [TIMEOUT_FEATURES.md](TIMEOUT_FEATURES.md)
- **完整 API** → [BCAST_GUIDE.md](BCAST_GUIDE.md) 的"API 参考"部分

### 我想看实际例子
- **基础用法** → `examples/bcast/coroutine_example.cpp`
- **超时** → `examples/bcast/timeout_example.cpp`
- **批量** → `examples/bcast/batch_example.cpp`
- **实战** → `examples/bcast/real_world_example.cpp`
- **性能** → `examples/bcast/benchmark.cpp`

## 文档更新历史

- 2025-10-10: 重组文档结构，合并多个文档为 BCAST_GUIDE.md
- 之前的文档：快速开始、使用指南、协程API指南、API对比已合并

## 贡献

如果你发现文档有任何问题或想要添加内容，欢迎贡献！

