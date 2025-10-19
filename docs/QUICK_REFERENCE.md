# 📚 ASIO-SRT 文档快速参考

一页式快速参考，帮助您快速找到所需文档。

## 🎯 我想...

### 开始使用项目
→ **[快速入门指南](guides/QUICK_START.md)** ⭐  
→ [SRT Socket 使用](guides/SRT_SOCKET_GUIDE.md)  
→ [示例代码](../examples/)

### 查看 API 文档
→ **[ACORE 组件 API](api/acore/README.md)** ⭐  
→ [异步原语](api/acore/ASYNC_PRIMITIVES.md)  
→ [取消机制](api/acore/CANCELLATION_SUPPORT.md)  
→ [WaitGroup 用法](api/acore/WAITGROUP_USAGE.md)

### 配置和定制
→ **[日志系统配置](design/logging/LOGGING_ENHANCED.md)** ⭐  
→ [回调和选项](guides/CALLBACK_AND_OPTIONS_GUIDE.md)  
→ [超时设置](design/timeout/TIMEOUT_API.md)

### 处理错误
→ **[错误处理指南](design/error-handling/ERROR_HANDLING.md)** ⭐  
→ [错误码说明](design/error-handling/ERROR_CODE_REFACTORING.md)  
→ [错误事件处理](design/error-handling/ERROR_EVENT_IMPLEMENTATION.md)

### 了解设计
→ **[设计文档索引](design/README.md)** ⭐  
→ [SRT 协议设计](design/srt/)  
→ [架构说明](STRUCTURE.md)

### 贡献代码
→ **[代码质量报告](development/code-reviews/FINAL_REPORT.md)** ⭐  
→ [项目状态](development/PROJECT_STATUS.md)  
→ [测试结果](development/TEST_RESULTS.md)

## 📂 按组件查找

### ACORE 异步组件
| 组件 | 文档 |
|------|------|
| **Semaphore** | [API](api/acore/ASYNC_PRIMITIVES.md#async_semaphore) / [详解](api/acore/ASYNC_SEMAPHORE_EXPLAINED.md) |
| **Queue** | [API](api/acore/ASYNC_PRIMITIVES.md#async_queue) / [简化说明](api/acore/ASYNC_QUEUE_SIMPLIFICATION.md) |
| **Event** | [API](api/acore/ASYNC_PRIMITIVES.md#async_event) / [分析](api/acore/ASYNC_EVENT_ANALYSIS.md) |
| **WaitGroup** | [API](api/acore/ASYNC_PRIMITIVES.md#async_waitgroup) / [用法](api/acore/WAITGROUP_USAGE.md) |
| **Dispatcher** | [API](api/acore/ASYNC_PRIMITIVES.md#dispatcher) / [变更](development/summaries/CHANGELOG_DISPATCHER.md) |

### ASRT (SRT 封装)
| 组件 | 文档 |
|------|------|
| **Reactor** | [错误处理](design/error-handling/) / [日志](design/logging/) |
| **Socket** | [使用指南](guides/SRT_SOCKET_GUIDE.md) / [选项](design/srt/SRT_OPTIONS_V2.md) |
| **Callbacks** | [原生回调](design/srt/SRT_NATIVE_CALLBACKS.md) |

## 📑 按主题查找

### 错误处理（7 篇）
- [ERROR_HANDLING.md](design/error-handling/ERROR_HANDLING.md) - 概览 ⭐
- [ERROR_CODE_REFACTORING.md](design/error-handling/ERROR_CODE_REFACTORING.md)
- [ERROR_EVENT_IMPLEMENTATION.md](design/error-handling/ERROR_EVENT_IMPLEMENTATION.md)
- [查看全部 →](design/error-handling/)

### 日志系统（2 篇）
- [LOGGING_ENHANCED.md](design/logging/LOGGING_ENHANCED.md) - 增强日志 ⭐
- [LOGGING_ENHANCEMENT.md](design/logging/LOGGING_ENHANCEMENT.md)

### 超时机制（2 篇）
- [TIMEOUT_API.md](design/timeout/TIMEOUT_API.md) - API 说明 ⭐
- [TIMEOUT_IMPLEMENTATION_COMPARISON.md](design/timeout/TIMEOUT_IMPLEMENTATION_COMPARISON.md)

### SRT 协议（3 篇）
- [SRT_NATIVE_CALLBACKS.md](design/srt/SRT_NATIVE_CALLBACKS.md)
- [SRT_OPTIONS_V2.md](design/srt/SRT_OPTIONS_V2.md)
- [SRT_V2_FEATURES.md](design/srt/SRT_V2_FEATURES.md)

### 代码审查（12 篇）
- [FINAL_REPORT.md](development/code-reviews/FINAL_REPORT.md) - 最终报告 ⭐
- [EXECUTIVE_SUMMARY.md](development/code-reviews/EXECUTIVE_SUMMARY.md) - 执行摘要 ⭐
- [ACORE_FULL_CODE_REVIEW.md](development/code-reviews/ACORE_FULL_CODE_REVIEW.md)
- [查看全部 →](development/code-reviews/)

### 开发总结（8 篇）
- [NEW_FEATURES.md](development/summaries/NEW_FEATURES.md)
- [IMPLEMENTATION_SUMMARY_V2.md](development/summaries/IMPLEMENTATION_SUMMARY_V2.md)
- [查看全部 →](development/summaries/)

## 🏷️ 按文档类型

| 类型 | 数量 | 入口 |
|------|------|------|
| 📘 **使用指南** | 8 篇 | [guides/](guides/) |
| 📗 **API 参考** | 9 篇 | [api/](api/) |
| 🏗️ **设计文档** | 16 篇 | [design/](design/) |
| 🔧 **开发文档** | 23 篇 | [development/](development/) |
| 📦 **归档文档** | 5 篇 | [archive/](archive/) |

## 🌟 推荐阅读路径

### 新手路径 (Day 1-3)
1. [快速入门](guides/QUICK_START.md) - 30 分钟
2. [ACORE API](api/acore/README.md) - 1 小时
3. [SRT Socket 指南](guides/SRT_SOCKET_GUIDE.md) - 1 小时
4. 运行示例代码 - 2 小时

### 进阶路径 (Week 1-2)
1. [异步原语详解](api/acore/ASYNC_PRIMITIVES.md)
2. [错误处理设计](design/error-handling/)
3. [日志系统配置](design/logging/LOGGING_ENHANCED.md)
4. [超时 API](design/timeout/TIMEOUT_API.md)

### 专家路径 (Month 1+)
1. [所有设计文档](design/)
2. [代码审查报告](development/code-reviews/)
3. [开发总结](development/summaries/)
4. 贡献代码和文档

## 🔍 搜索提示

### 按关键词
- **协程** → [COROUTINE_ONLY.md](api/acore/COROUTINE_ONLY.md)
- **取消** → [CANCELLATION_SUPPORT.md](api/acore/CANCELLATION_SUPPORT.md)
- **错误** → [error-handling/](design/error-handling/)
- **日志** → [logging/](design/logging/)
- **超时** → [timeout/](design/timeout/)
- **队列** → [ASYNC_PRIMITIVES.md](api/acore/ASYNC_PRIMITIVES.md) + [QUEUE_COMPARISON.md](design/QUEUE_COMPARISON.md)

### 按功能
- **并发控制** → Semaphore、WaitGroup
- **消息传递** → Queue、Event
- **任务调度** → Dispatcher
- **网络通信** → SRT Socket、Reactor

## 📌 常用链接

- [主文档索引](README.md) - 完整文档列表
- [项目结构](STRUCTURE.md) - 目录组织说明
- [项目主页](../README.md) - 返回主页
- [示例代码](../examples/) - 实际代码示例
- [测试说明](../tests/README.md) - 测试文档

## 💡 提示

- ⭐ 标记表示推荐优先阅读的文档
- 每个目录都有 README.md 索引
- 使用浏览器的搜索功能查找关键词
- 文档间有相互链接，可以跟随链接深入阅读

---

**快速参考版本**: 1.0  
**上次更新**: 2025-10-18


