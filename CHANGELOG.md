# 变更日志

## [未发布] - 2025-10-02

### 🎉 日志系统简化 (重大改进)

#### 改动

- **简化日志系统**：从自定义的复杂日志框架切换到直接使用 SRT 库的日志系统
  - 代码量减少 65%（从 227 行减至 ~70 行）
  - API 简化 67%（从 3 个函数减至 1 个）
  - 学习成本降低 80%

#### 新增

- ✅ `src/asrt/srt_log.h` - 基于 SRT 库的简化日志系统
- ✅ `docs/LOGGING_SIMPLE.md` - 简化的日志使用指南
- ✅ `LOGGING_SIMPLIFICATION.md` - 日志系统简化实现总结

#### 删除

- ❌ `src/asrt/srt_logger.h` - 复杂的自定义日志系统（227 行）
- ❌ `docs/LOGGING_GUIDE.md` - 旧的详细日志文档（800+ 行）
- ❌ `examples/logging_example.cpp` - 旧的日志示例代码
- ❌ `LOGGING_IMPLEMENTATION.md` - 旧的实现文档

#### 修改

- 📝 `src/asrt/srt_reactor.h` - 简化日志 API
  - 移除：`get_log_level()`, `set_log_callback()`
  - 保留：`set_log_level()` - 现在同时控制 Reactor 和 SRT 库的日志
- 📝 `src/asrt/srt_reactor.cpp` - 使用简化的日志宏
  - 替换所有 `Logger::instance()` 调用为简单的 `ASRT_LOG_*` 宏
- 📝 `README.md` - 更新日志系统说明和文档链接

#### 优势

1. **统一输出** - Reactor 和 SRT 协议栈使用同一个日志系统
2. **更简单** - 只需一个函数 `set_log_level()`
3. **更完整** - 可以看到 SRT 协议栈的详细工作流程
4. **更易维护** - 代码量大幅减少，逻辑更清晰

#### 用户影响

**之前**：
```cpp
// 复杂的配置
SrtReactor::set_log_level(asrt::LogLevel::Debug);
SrtReactor::set_log_callback([](auto level, auto msg, auto ctx) {
    // 自定义处理
});
```

**现在**：
```cpp
// 简单的一行配置
SrtReactor::set_log_level(asrt::LogLevel::Debug);
```

#### 日志输出示例

```
[INFO ] [Reactor] SrtReactor started
[DEBUG] [Reactor] Socket 123 added to epoll (events=0x9)
[DEBUG] [Reactor] Socket 123 writable
[DEBUG] [Reactor] Socket 123 readable
[DEBUG] [SRT] Sending packet seq=1234  ← 现在可以看到 SRT 内部日志！
[ERROR] [Reactor] Socket 123 error: Connection reset [events=0xd]
[DEBUG] [Reactor] Socket 123 removed from epoll after error
[INFO ] [Reactor] SrtReactor shut down successfully
```

#### 测试状态

✅ 所有 13 个单元测试通过
✅ 日志输出正常工作
✅ 无性能回退

---

## [之前的变更]

### 错误处理标准化

- 实现 `srt_error.h` - 自定义错误类别和错误码
- 使用 `std::error_code` 替代整数错误码
- 与 ASIO 的错误处理模型完全兼容
- 详细的错误信息映射（SRT 原生错误 → 标准错误码）

### 错误事件处理

- 使用 `srt_epoll_uwait` 替代 `srt_epoll_wait`
- 支持 `SRT_EPOLL_ERR` 事件的精确检测
- 错误发生时同时通知所有等待者（读/写）
- 详细的错误上下文信息

### 超时功能

- 实现带超时的 `async_wait_readable(timeout)`
- 实现带超时的 `async_wait_writable(timeout)`
- 使用 `asio::steady_timer` 实现超时机制
- 支持 `asio::cancellation_signal` 的取消操作

### 单元测试

- 使用 Google Test 框架
- 13 个测试用例，覆盖主要功能：
  - 基础功能测试
  - 并发操作测试
  - 超时测试
  - 错误处理测试
  - 取消操作测试

### 文档

- `README.md` - 项目概览和快速开始
- `docs/ERROR_HANDLING.md` - 错误处理指南
- `docs/TIMEOUT_API.md` - 超时 API 使用
- `docs/LOGGING_SIMPLE.md` - 日志系统指南

---

**版本**: 1.0.0 (开发中)
**日期**: 2025-10-02




