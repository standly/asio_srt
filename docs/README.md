# ASIO-SRT 文档索引

本目录包含 ASIO-SRT 项目的所有文档。

## 📚 文档结构

### [guides/](guides/) - 使用指南
用户和开发者的实用指南：
- [快速入门](guides/QUICK_START.md) - 快速开始使用 ASIO-SRT
- [回调和选项指南](guides/CALLBACK_AND_OPTIONS_GUIDE.md) - 回调函数和配置选项详解
- [SRT Socket 指南](guides/SRT_SOCKET_GUIDE.md) - SRT Socket 使用指南
- [bcast/](guides/bcast/) - 广播（Broadcast）功能相关指南

### [api/](api/) - API 文档
组件 API 参考文档：
- [acore/](api/acore/) - 异步核心组件 API
  - [异步原语](api/acore/ASYNC_PRIMITIVES.md) - 异步信号量、队列、事件等
  - [协程模式](api/acore/COROUTINE_ONLY.md) - 协程专用 API 说明
  - [取消支持](api/acore/CANCELLATION_SUPPORT.md) - 取消机制详解
  - [WaitGroup 用法](api/acore/WAITGROUP_USAGE.md) - WaitGroup 使用指南
- [asrt/](api/asrt/) - SRT 相关 API（待添加）

### [design/](design/) - 设计文档
系统设计和架构文档：
- [error-handling/](design/error-handling/) - 错误处理设计
- [logging/](design/logging/) - 日志系统设计
- [timeout/](design/timeout/) - 超时机制设计
- [srt/](design/srt/) - SRT 协议相关设计

### [development/](development/) - 开发文档
开发过程和测试相关文档：
- [code-reviews/](development/code-reviews/) - 代码审查报告
- [summaries/](development/summaries/) - 各类开发总结
- [测试结果](development/TEST_RESULTS.md) - 测试报告
- [项目状态](development/PROJECT_STATUS.md) - 项目当前状态

## 🚀 快速导航

- **新手入门**: 从 [快速入门指南](guides/QUICK_START.md) 开始
- **API 查询**: 查看 [acore API 文档](api/acore/)
- **功能特性**: 参考 [设计文档](design/)
- **开发贡献**: 阅读 [开发文档](development/)

## 📝 文档约定

- **guides/** - 面向用户的实用指南，包含示例和最佳实践
- **api/** - API 参考文档，详细说明接口和用法
- **design/** - 设计决策和架构说明
- **development/** - 开发过程文档，包括测试、审查和总结

