# 使用指南

本目录包含 ASIO-SRT 项目的实用指南和教程。

## 📖 核心指南

| 指南 | 说明 | 适合人群 |
|------|------|----------|
| [快速入门](QUICK_START.md) | 5分钟快速开始使用 | 🌟 新用户必读 |
| [SRT Socket 指南](SRT_SOCKET_GUIDE.md) | SRT Socket 详细使用方法 | 中级用户 |
| [回调和选项](CALLBACK_AND_OPTIONS_GUIDE.md) | 回调函数和配置选项详解 | 高级用户 |

## 📚 专题指南

### [Broadcast (广播)](bcast/)

广播功能的使用指南：
- [BCAST_GUIDE.md](bcast/BCAST_GUIDE.md) - 广播功能完整指南
- [BATCH_OPERATIONS.md](bcast/BATCH_OPERATIONS.md) - 批量操作指南
- [TIMEOUT_FEATURES.md](bcast/TIMEOUT_FEATURES.md) - 超时功能使用
- [README.md](bcast/README.md) - Broadcast 概览

## 🎯 学习路径

### 新手路径 (0 → 基础)
1. 阅读 [快速入门](QUICK_START.md)
2. 运行示例代码（`examples/` 目录）
3. 阅读 [SRT Socket 指南](SRT_SOCKET_GUIDE.md)

### 进阶路径 (基础 → 熟练)
1. 学习 [回调和选项](CALLBACK_AND_OPTIONS_GUIDE.md)
2. 阅读 [ACORE API 文档](../api/acore/)
3. 了解 [错误处理设计](../design/error-handling/)

### 高级路径 (熟练 → 专家)
1. 研究 [设计文档](../design/)
2. 查看 [代码审查报告](../development/code-reviews/)
3. 贡献代码和文档

## 💡 常见场景

### 场景：我想发送视频流
→ 阅读 [SRT Socket 指南](SRT_SOCKET_GUIDE.md)  
→ 参考 `examples/srt_streaming_example.cpp`

### 场景：我想处理连接错误
→ 阅读 [错误处理指南](../design/error-handling/ERROR_HANDLING.md)  
→ 查看 [SRT Socket 指南](SRT_SOCKET_GUIDE.md) 的错误处理部分

### 场景：我想配置日志
→ 阅读 [日志系统指南](../design/logging/LOGGING_ENHANCED.md)  
→ 参考 `examples/custom_logging_example.cpp`

### 场景：我想使用异步队列
→ 阅读 [ACORE 异步原语](../api/acore/ASYNC_PRIMITIVES.md)  
→ 查看 `examples/acore/` 中的示例

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
