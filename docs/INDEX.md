# 📚 ASIO-SRT 完整文档索引

完整的项目文档导航和快速查找指南。

---

## 🎯 快速导航

| 我想... | 文档链接 |
|---------|---------|
| **快速开始** | [快速入门](guides/QUICK_START.md) ⭐ |
| **查看 API** | [ACORE API](api/acore/README.md) ⭐ |
| **了解设计** | [设计文档](design/README.md) |
| **查看示例** | [examples/](../examples/) |
| **运行测试** | [tests/](../tests/) |

---

## 📖 文档分类

### 1. 使用指南 (guides/)

新用户必读！

| 文档 | 说明 | 优先级 |
|------|------|--------|
| [QUICK_START.md](guides/QUICK_START.md) | 快速入门 | ⭐⭐⭐ |
| [SRT_SOCKET_GUIDE.md](guides/SRT_SOCKET_GUIDE.md) | SRT Socket 使用指南 | ⭐⭐⭐ |
| [CALLBACK_AND_OPTIONS_GUIDE.md](guides/CALLBACK_AND_OPTIONS_GUIDE.md) | 回调和选项配置 | ⭐⭐ |

### 2. API 参考 (api/)

#### ACORE 组件 API
- [ACORE README](api/acore/README.md) - 组件概览 ⭐
- [ASYNC_PRIMITIVES.md](api/acore/ASYNC_PRIMITIVES.md) - 异步原语详解
- [CANCELLATION_SUPPORT.md](api/acore/CANCELLATION_SUPPORT.md) - 取消机制
- [WAITGROUP_USAGE.md](api/acore/WAITGROUP_USAGE.md) - WaitGroup 用法

#### 新增组件文档
- [src/acore/README.md](../src/acore/README.md) - 所有异步组件完整文档 ⭐⭐⭐

### 3. 设计文档 (design/)

了解技术决策和实现细节。

#### 错误处理 (design/error-handling/)
- [ERROR_HANDLING.md](design/error-handling/ERROR_HANDLING.md) - 概览 ⭐
- [ERROR_CODE_REFACTORING.md](design/error-handling/ERROR_CODE_REFACTORING.md)
- [ERROR_EVENT_IMPLEMENTATION.md](design/error-handling/ERROR_EVENT_IMPLEMENTATION.md)

#### 日志系统 (design/logging/)
- [LOGGING_ENHANCED.md](design/logging/LOGGING_ENHANCED.md) - 日志系统指南 ⭐

#### 超时机制 (design/timeout/)
- [TIMEOUT_API.md](design/timeout/TIMEOUT_API.md) - 超时 API 说明 ⭐

#### SRT 协议 (design/srt/)
- [SRT_NATIVE_CALLBACKS.md](design/srt/SRT_NATIVE_CALLBACKS.md)
- [SRT_OPTIONS_V2.md](design/srt/SRT_OPTIONS_V2.md)
- [SRT_V2_FEATURES.md](design/srt/SRT_V2_FEATURES.md)

### 4. 开发文档 (development/)

#### 项目状态
- [PROJECT_STATUS.md](development/PROJECT_STATUS.md) - 当前状态 ⭐

#### 代码审查 (development/code-reviews/)
- [FINAL_REPORT.md](development/code-reviews/FINAL_REPORT.md) - 最终报告 ⭐
- [EXECUTIVE_SUMMARY.md](development/code-reviews/EXECUTIVE_SUMMARY.md)

#### ACORE 组件开发 (development/acore/)
- [IMPLEMENTATION_SUMMARY.md](development/acore/IMPLEMENTATION_SUMMARY.md) - 新组件实现总结 ⭐
- [README.md](development/acore/README.md)
- [README_TESTS.md](development/acore/README_TESTS.md)

#### 项目重组 (development/reorganization/)
- [README.md](development/reorganization/README.md) - 重组总结 ⭐
- [PROJECT_CLEANUP_COMPLETE.md](development/reorganization/PROJECT_CLEANUP_COMPLETE.md)
- [DIRECTORY_CLEANUP_SUMMARY.md](development/reorganization/DIRECTORY_CLEANUP_SUMMARY.md)
- [TESTS_REORGANIZATION.md](development/reorganization/TESTS_REORGANIZATION.md)

---

## 📊 按组件查找

### ACORE 异步组件

#### 原有组件
| 组件 | API 文档 | 测试 | 示例 |
|------|---------|------|------|
| async_semaphore | [src/acore/README.md](../src/acore/README.md#async_semaphore) | [test](../tests/acore/test_async_semaphore.cpp) | [example](../examples/acore/semaphore_test.cpp) |
| async_queue | [src/acore/README.md](../src/acore/README.md#async_queue) | [test](../tests/acore/test_async_queue.cpp) | [example](../examples/acore/example.cpp) |
| async_event | [src/acore/README.md](../src/acore/README.md#async_event) | [test](../tests/acore/test_async_event.cpp) | [example](../examples/acore/event_test.cpp) |
| async_waitgroup | [src/acore/README.md](../src/acore/README.md#async_waitgroup) | [test](../tests/acore/test_waitgroup.cpp) | [example](../examples/acore/example_waitgroup_simple.cpp) |
| dispatcher | [src/acore/README.md](../src/acore/README.md#dispatcher) | [test](../tests/acore/test_dispatcher.cpp) | [example](../examples/acore/advanced_example.cpp) |

#### 新增组件 (2025-10-19)
| 组件 | API 文档 | 测试 | 示例 |
|------|---------|------|------|
| async_mutex | [src/acore/README.md](../src/acore/README.md#async_mutex) | [test](../tests/acore/test_async_mutex.cpp) | [example](../examples/acore/mutex_example.cpp) |
| async_periodic_timer | [src/acore/README.md](../src/acore/README.md#async_periodic_timer) | [test](../tests/acore/test_async_periodic_timer.cpp) | [example](../examples/acore/timer_example.cpp) |
| async_rate_limiter | [src/acore/README.md](../src/acore/README.md#async_rate_limiter) | [test](../tests/acore/test_async_rate_limiter.cpp) | [example](../examples/acore/rate_limiter_example.cpp) |
| async_barrier | [src/acore/README.md](../src/acore/README.md#async_barrier) | [test](../tests/acore/test_async_barrier.cpp) | [example](../examples/acore/barrier_latch_example.cpp) |
| async_auto_reset_event | [src/acore/README.md](../src/acore/README.md#async_auto_reset_event) | [test](../tests/acore/test_async_auto_reset_event.cpp) | - |
| async_latch | [src/acore/README.md](../src/acore/README.md#async_latch) | [test](../tests/acore/test_async_latch.cpp) | [example](../examples/acore/barrier_latch_example.cpp) |

### ASRT (SRT 集成)
| 组件 | 文档 | 示例 |
|------|------|------|
| SrtReactor | [PROJECT_STATUS.md](development/PROJECT_STATUS.md) | [srt_server_example.cpp](../examples/srt_server_example.cpp) |
| SrtSocket | [guides/SRT_SOCKET_GUIDE.md](guides/SRT_SOCKET_GUIDE.md) | [srt_client_example.cpp](../examples/srt_client_example.cpp) |

---

## 📁 目录结构说明

```
docs/
├── INDEX.md                    # 本文件（文档索引）
├── README.md                   # 文档中心
├── QUICK_REFERENCE.md          # 快速参考
├── STRUCTURE.md                # 项目结构
│
├── api/                        # API 参考文档
│   └── acore/                  # ACORE 组件 API
│
├── guides/                     # 使用指南
│   ├── QUICK_START.md         # 快速入门
│   └── SRT_SOCKET_GUIDE.md    # SRT Socket 指南
│
├── design/                     # 设计文档
│   ├── error-handling/        # 错误处理设计
│   ├── logging/               # 日志系统设计
│   ├── timeout/               # 超时机制设计
│   └── srt/                   # SRT 协议设计
│
├── development/                # 开发文档
│   ├── PROJECT_STATUS.md      # 项目状态
│   ├── acore/                 # ACORE 开发文档
│   ├── reorganization/        # 项目重组文档
│   ├── code-reviews/          # 代码审查
│   └── summaries/             # 开发总结
│
└── archive/                    # 归档文档
```

---

## 🔍 搜索提示

### 按关键词搜索
- **协程** → [COROUTINE_ONLY.md](api/acore/COROUTINE_ONLY.md)
- **取消** → [CANCELLATION_SUPPORT.md](api/acore/CANCELLATION_SUPPORT.md)
- **错误** → [design/error-handling/](design/error-handling/)
- **日志** → [design/logging/](design/logging/)
- **超时** → [design/timeout/](design/timeout/)
- **互斥锁** → [src/acore/README.md#async_mutex](../src/acore/README.md)
- **定时器** → [src/acore/README.md#async_periodic_timer](../src/acore/README.md)
- **限流** → [src/acore/README.md#async_rate_limiter](../src/acore/README.md)

### 按功能搜索
- **并发控制** → async_mutex, async_semaphore
- **同步原语** → async_barrier, async_latch, async_waitgroup
- **事件通知** → async_event, async_auto_reset_event
- **消息传递** → async_queue, dispatcher
- **定时任务** → async_periodic_timer, async_timer
- **流量控制** → async_rate_limiter

---

## 📈 文档统计

| 类型 | 数量 | 位置 |
|------|------|------|
| 使用指南 | 8+ | guides/ |
| API 文档 | 10+ | api/ |
| 设计文档 | 15+ | design/ |
| 开发文档 | 20+ | development/ |
| 归档文档 | 5+ | archive/ |
| **总计** | **58+** | - |

---

## ✨ 推荐阅读顺序

### 新手（第 1-3 天）
1. [README.md](../README.md) - 30分钟
2. [QUICK_START.md](guides/QUICK_START.md) - 1小时
3. [ACORE README](../src/acore/README.md) - 1小时
4. 运行示例代码 - 2小时

### 进阶（第 1-2 周）
1. [ASYNC_PRIMITIVES.md](api/acore/ASYNC_PRIMITIVES.md)
2. [设计文档](design/)
3. [SRT_SOCKET_GUIDE.md](guides/SRT_SOCKET_GUIDE.md)

### 专家（1个月+）
1. 所有设计文档
2. 代码审查报告
3. 贡献代码

---

## 🔗 相关资源

- [项目主页](../README.md)
- [快速参考](QUICK_REFERENCE.md)
- [项目结构](STRUCTURE.md)
- [源代码](../src/)
- [示例代码](../examples/)
- [测试代码](../tests/)

---

**文档索引版本**: 2.0  
**最后更新**: 2025-10-19  
**文档总数**: 58+

