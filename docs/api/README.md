# API 参考文档

本目录包含 ASIO-SRT 项目各组件的详细 API 文档。

## 📚 组件列表

### [ACORE - 异步核心组件](acore/)

基于 Boost.Asio 的高级异步原语库。

**主要组件**：
- **async_semaphore** - 异步信号量
- **async_queue** - 异步队列
- **async_event** - 异步事件
- **async_waitgroup** - WaitGroup
- **dispatcher** - 任务调度器

**文档入口**: [acore/README.md](acore/README.md) ⭐

**核心文档**：
- [异步原语总览](acore/ASYNC_PRIMITIVES.md) - 所有组件的使用说明
- [协程模式](acore/COROUTINE_ONLY.md) - C++20 协程 API
- [取消机制](acore/CANCELLATION_SUPPORT.md) - 操作取消支持
- [WaitGroup 用法](acore/WAITGROUP_USAGE.md) - WaitGroup 详细指南

**深度分析**：
- [Event 分析](acore/ASYNC_EVENT_ANALYSIS.md)
- [Queue 简化](acore/ASYNC_QUEUE_SIMPLIFICATION.md)
- [Semaphore 详解](acore/ASYNC_SEMAPHORE_EXPLAINED.md)

### ASRT - SRT 协议封装

SRT 协议的异步封装（文档待完善）。

## 🚀 快速开始

1. 选择您需要的组件
2. 阅读对应的 API 文档
3. 查看示例代码（`examples/` 目录）
4. 开始使用！

## 🎯 按功能查找

### 并发控制
- **Semaphore** → [ASYNC_PRIMITIVES.md](acore/ASYNC_PRIMITIVES.md#async_semaphore)
- **WaitGroup** → [WAITGROUP_USAGE.md](acore/WAITGROUP_USAGE.md)

### 消息传递
- **Queue** → [ASYNC_PRIMITIVES.md](acore/ASYNC_PRIMITIVES.md#async_queue)
- **Event** → [ASYNC_PRIMITIVES.md](acore/ASYNC_PRIMITIVES.md#async_event)

### 任务调度
- **Dispatcher** → [ASYNC_PRIMITIVES.md](acore/ASYNC_PRIMITIVES.md#dispatcher)

## 📖 文档约定

- **入门** - README 和 ASYNC_PRIMITIVES
- **进阶** - CANCELLATION_SUPPORT、COROUTINE_ONLY
- **深入** - 各组件的分析文档

## 🔗 相关链接

- [使用指南](../guides/) - 实用指南和教程
- [设计文档](../design/) - 设计决策
- [源代码](../../src/acore/) - 查看源码
- [示例代码](../../examples/acore/) - 实际示例
