# 使用指南

本目录包含 ASIO-SRT 项目的实用指南和教程。

## 📖 核心指南

| 指南 | 说明 | 适合人群 |
|------|------|----------|
| [快速入门](QUICK_START.md) | 5分钟快速开始使用 | 🌟 新用户必读 |
| [ACORE 组件指南](ACORE_GUIDE.md) | 异步组件完整使用指南 | ⭐ 推荐阅读 |
| [SRT Socket 指南](SRT_SOCKET_GUIDE.md) | SRT Socket 详细使用方法 | 中级用户 |
| [回调和选项](CALLBACK_AND_OPTIONS_GUIDE.md) | 回调函数和配置选项详解 | 高级用户 |

## 📚 ACORE 异步组件

**[ACORE 完整指南](ACORE_GUIDE.md)** - 所有 12 个异步组件的使用说明 ⭐⭐⭐

包含：
- 🔒 同步原语（mutex, semaphore, barrier）
- 📢 事件通知（event, auto_reset_event）
- 🔢 计数器（waitgroup, latch）
- 📬 消息传递（queue, dispatcher）
- ⏱️ 定时器（periodic_timer, timer）
- 🚦 流量控制（rate_limiter）

## 🎯 学习路径

### 新手路径 (0 → 基础)
1. 阅读 [快速入门](QUICK_START.md)
2. 学习 [ACORE 组件指南](ACORE_GUIDE.md) ⭐
3. 运行示例代码（`examples/acore/` 目录）

### 进阶路径 (基础 → 熟练)
1. 阅读 [SRT Socket 指南](SRT_SOCKET_GUIDE.md)
2. 学习 [回调和选项](CALLBACK_AND_OPTIONS_GUIDE.md)
3. 了解 [错误处理设计](../design/error-handling/)

### 高级路径 (熟练 → 专家)
1. 研究 [设计文档](../design/)
2. 查看 [代码审查报告](../development/code-reviews/)
3. 贡献代码和文档

## 💡 常见场景

### 场景：我想保护共享资源
→ 使用 **async_mutex**  
→ 阅读 [ACORE 组件指南](ACORE_GUIDE.md#async_mutex)  
→ 参考 `examples/acore/mutex_example.cpp`

### 场景：我想限制并发数
→ 使用 **async_semaphore**  
→ 阅读 [ACORE 组件指南](ACORE_GUIDE.md#async_semaphore)

### 场景：我想限制API调用频率
→ 使用 **async_rate_limiter**  
→ 阅读 [ACORE 组件指南](ACORE_GUIDE.md#async_rate_limiter)  
→ 参考 `examples/acore/rate_limiter_example.cpp`

### 场景：我想周期性发送心跳
→ 使用 **async_periodic_timer**  
→ 阅读 [ACORE 组件指南](ACORE_GUIDE.md#async_periodic_timer)  
→ 参考 `examples/acore/timer_example.cpp`

### 场景：我想实现消息队列
→ 使用 **async_queue** 或 **dispatcher**  
→ 阅读 [ACORE 组件指南](ACORE_GUIDE.md#消息传递)  
→ 参考 `examples/acore/example.cpp`

### 场景：我想等待多个任务完成
→ 使用 **async_waitgroup** 或 **async_latch**  
→ 阅读 [ACORE 组件指南](ACORE_GUIDE.md#计数器)

### 场景：我想发送视频流
→ 阅读 [SRT Socket 指南](SRT_SOCKET_GUIDE.md)  
→ 参考 `examples/srt_streaming_example.cpp`

### 场景：我想处理连接错误
→ 阅读 [错误处理指南](../design/error-handling/ERROR_HANDLING.md)  
→ 查看 [SRT Socket 指南](SRT_SOCKET_GUIDE.md) 的错误处理部分

### 场景：我想配置日志
→ 阅读 [日志系统指南](../design/logging/LOGGING_ENHANCED.md)  
→ 参考 `examples/custom_logging_example.cpp`

## 🔧 实用工具

### 调试技巧
1. 启用详细日志：`SrtReactor::set_log_level(LogLevel::Debug)`
2. 使用错误码检查：检查 `std::error_code`
3. 启用协程异常追踪

### 性能优化
1. 合理使用信号量控制并发
2. 使用 `async_queue` 进行流控
3. 避免阻塞操作

### 最佳实践
1. 总是处理错误
2. 合理设置超时
3. 使用 RAII 管理资源
4. 遵循协程生命周期规则

## 📝 贡献指南

欢迎补充新的使用指南！添加新指南时：

1. 放置在 `guides/` 目录
2. 使用清晰的文件命名（如 `FEATURE_NAME_GUIDE.md`）
3. 包含实际代码示例
4. 更新本 README

## 🔗 相关资源

- [API 文档](../api/) - 详细的 API 参考
- [设计文档](../design/) - 设计决策说明
- [示例代码](../../examples/) - 实际代码示例
- [项目主页](../../README.md) - 返回主页
