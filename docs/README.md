# ASIO-SRT 项目文档

欢迎查看 ASIO-SRT 项目文档！本文档中心提供了完整的项目信息。

## 🚀 快速开始

**新用户从这里开始**：
- 📖 [快速入门指南](guides/QUICK_START.md) - 5分钟快速上手
- 🏗️ [项目结构](STRUCTURE.md) - 了解项目组织
- 💡 [使用示例](../examples/) - 查看实际代码示例

## 📚 文档导航

### 1️⃣ [使用指南](guides/) - 实践指南

面向用户的实用文档，包含使用方法和最佳实践：

| 文档 | 说明 |
|------|------|
| [快速入门](guides/QUICK_START.md) | 快速开始使用 ASIO-SRT |
| [SRT Socket 指南](guides/SRT_SOCKET_GUIDE.md) | SRT Socket 详细使用方法 |
| [回调和选项](guides/CALLBACK_AND_OPTIONS_GUIDE.md) | 回调函数和配置选项 |
| [Broadcast 指南](guides/bcast/) | 广播功能使用指南 |

### 2️⃣ [API 参考](api/) - 组件 API

各组件的详细 API 文档：

#### ACORE - 异步核心组件
- [组件概览](api/acore/README.md) - ACORE 组件总览
- [异步原语](api/acore/ASYNC_PRIMITIVES.md) - Semaphore、Queue、Event、WaitGroup
- [取消机制](api/acore/CANCELLATION_SUPPORT.md) - 操作取消支持
- [协程模式](api/acore/COROUTINE_ONLY.md) - C++20 协程 API
- [WaitGroup 用法](api/acore/WAITGROUP_USAGE.md) - WaitGroup 详细指南
- [组件分析](api/acore/) - 组件实现分析（ASYNC_EVENT_ANALYSIS、ASYNC_QUEUE_SIMPLIFICATION 等）

#### ASRT - SRT 封装
- 待完善

### 3️⃣ [设计文档](design/) - 架构设计

系统设计决策和架构说明：

#### 错误处理
- [错误处理概览](design/error-handling/ERROR_HANDLING.md)
- [错误码设计](design/error-handling/ERROR_CODE_REFACTORING.md)
- [错误事件实现](design/error-handling/ERROR_EVENT_IMPLEMENTATION.md)
- [更多...](design/error-handling/)

#### 日志系统
- [增强日志系统](design/logging/LOGGING_ENHANCED.md)
- [日志增强说明](design/logging/LOGGING_ENHANCEMENT.md)

#### 超时机制
- [超时 API](design/timeout/TIMEOUT_API.md)
- [超时实现对比](design/timeout/TIMEOUT_IMPLEMENTATION_COMPARISON.md)

#### SRT 协议
- [SRT 原生回调](design/srt/SRT_NATIVE_CALLBACKS.md)
- [SRT 选项 V2](design/srt/SRT_OPTIONS_V2.md)
- [SRT V2 新功能](design/srt/SRT_V2_FEATURES.md)

#### 其他设计
- [队列对比](design/QUEUE_COMPARISON.md)
- [重组方案](design/REORGANIZATION.md)

### 4️⃣ [开发文档](development/) - 开发过程

开发过程文档、测试和审查报告：

| 类型 | 文档 |
|------|------|
| **项目状态** | [PROJECT_STATUS.md](development/PROJECT_STATUS.md) |
| **测试报告** | [TEST_RESULTS.md](development/TEST_RESULTS.md) |
| **代码审查** | [code-reviews/](development/code-reviews/) - 8 份主要审查报告 |
| **开发总结** | [summaries/](development/summaries/) - 8 份开发总结 |

主要审查报告：
- [ACORE 完整审查](development/code-reviews/ACORE_FULL_CODE_REVIEW.md)
- [所有组件最终审查](development/code-reviews/FINAL_CODE_REVIEW_ALL_COMPONENTS.md)
- [最终报告](development/code-reviews/FINAL_REPORT.md)
- [执行摘要](development/code-reviews/EXECUTIVE_SUMMARY.md)

### 5️⃣ [归档文档](archive/) - 历史文档

历史文档和重组记录（不常用）

## 📊 文档统计

- **使用指南**: 8 篇（含 Broadcast 子目录）
- **API 文档**: 9 篇
- **设计文档**: 16 篇
- **开发文档**: 19 篇（含审查和总结）
- **归档文档**: 5 篇

**总计**: 57+ 篇文档

## 🎯 按场景查找

### 我想了解...

**如何使用这个项目**
→ [快速入门](guides/QUICK_START.md)
→ [SRT Socket 指南](guides/SRT_SOCKET_GUIDE.md)

**ACORE 组件的 API**
→ [ACORE API](api/acore/)
→ [异步原语](api/acore/ASYNC_PRIMITIVES.md)

**错误处理如何工作**
→ [错误处理设计](design/error-handling/)

**日志系统如何配置**
→ [日志系统指南](design/logging/LOGGING_ENHANCED.md)

**项目的开发状态**
→ [项目状态](development/PROJECT_STATUS.md)
→ [测试结果](development/TEST_RESULTS.md)

**代码质量如何**
→ [代码审查报告](development/code-reviews/)

## 🔧 贡献文档

添加新文档时，请遵循以下规则：

1. **使用指南** → `guides/` - 用户实践文档
2. **API 文档** → `api/{component}/` - API 参考
3. **设计文档** → `design/{topic}/` - 设计决策
4. **开发文档** → `development/` - 开发过程文档
5. **归档文档** → `archive/` - 过时或历史文档

每次添加文档后，请更新相应目录的 README.md 索引。

## 📞 需要帮助？

- 💬 提交 [Issue](../issues)
- 📧 查看 [项目主页](../README.md)
- 🔍 浏览 [示例代码](../examples/)

---

**上次更新**: 2025-10-18  
**文档版本**: 2.0
