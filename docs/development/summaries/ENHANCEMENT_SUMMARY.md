# asio_srt 功能增强总结

## 1. 日志系统增强

### 新增功能
- **调用位置记录**：日志现在会自动记录打印位置的文件名、函数名和行号
- **增强的日志回调**：`LogCallback` 签名更新，新增 `file`、`function` 和 `line` 参数

### API 变更
```cpp
// 旧版本
using LogCallback = std::function<void(LogLevel level, const char* area, const char* message)>;

// 新版本
using LogCallback = std::function<void(LogLevel level, const char* area, const char* message,
                                      const char* file, const char* function, int line)>;
```

### 使用示例
```cpp
// 设置自定义日志回调
SrtReactor::set_log_callback(
    [](LogLevel level, const char* area, const char* message,
       const char* file, const char* function, int line) {
        std::cout << "[" << file << ":" << function << ":" << line << "] " 
                  << message << std::endl;
    }
);
```

### 日志宏
所有日志宏（`ASRT_LOG_DEBUG`、`ASRT_LOG_INFO`、`ASRT_LOG_WARNING`、`ASRT_LOG_ERROR`）现在会自动传递调用位置信息。

## 2. Callback 处理

### 验证结果
- **Listener Callback**：已确认直接透传给 SRT 协议栈，通过 `srt_listen_callback` 函数设置
- **Connect Callback**：已确认直接透传给 SRT 协议栈，通过 `srt_connect_callback` 函数设置

无需额外处理，callback 机制已经是透传的。

## 3. 新增单元测试

### test_logging_enhancement.cpp
测试日志增强功能的单元测试，包括：
- 验证日志包含文件名、函数名和行号
- 不同日志级别的文件信息测试
- 自定义区域的日志测试
- 日志格式化输出测试
- 文件路径处理测试
- 并发日志记录测试
- SRT库日志的文件信息测试

### test_srt_socket_acceptor.cpp
测试 SrtSocket 和 SrtAcceptor 功能的单元测试，包括：
- 基本的连接和接受测试
- Listener callback 测试
- 连接拒绝测试
- Socket 选项设置和获取测试
- 数据发送和接收测试
- 连接超时测试
- 多个并发连接测试
- Connect callback 测试
- 地址获取功能测试
- 错误处理测试

## 4. 新增示例程序

### srt_streaming_example.cpp
展示 SRT 流媒体传输的综合示例，包括：
- **视频流服务器**：模拟实时视频流传输，支持多客户端
- **视频流客户端**：接收视频流并显示统计信息
- **双向音视频通话**：演示全双工通信
- **多路复用广播服务器**：一对多的广播场景

使用方法：
```bash
# 视频流服务器
./srt_streaming_example server <port>

# 视频流客户端
./srt_streaming_example client <server> <port> <stream_id>

# 双向通话
./srt_streaming_example call-caller <local_port> <remote_addr> <remote_port>
./srt_streaming_example call-callee <local_port> <remote_addr> <remote_port>

# 广播服务器
./srt_streaming_example broadcast <port>
```

### srt_resilient_client_example.cpp
展示错误处理和重连机制的高可靠性客户端，特性包括：
- **自动重连**：使用指数退避算法的智能重连机制
- **错误恢复**：优雅处理各种网络错误
- **心跳机制**：保持连接活跃并检测连接状态
- **实时统计**：显示 RTT、带宽、发送/接收速率等信息
- **线程安全**：支持多线程环境下的数据发送

### custom_logging_example.cpp (更新)
更新以支持新的日志回调签名，展示了：
- 自定义格式输出（包含文件位置信息）
- 输出到文件（包含完整调用栈信息）
- 集成第三方日志库的示例
- 按区域过滤日志
- JSON 格式的结构化日志输出

## 5. 编译系统更新

### tests/CMakeLists.txt
- 添加了 `test_logging_enhancement` 测试目标
- 添加了 `test_srt_socket_acceptor` 测试目标

### examples/CMakeLists.txt
- 添加了 `srt_streaming_example` 示例目标
- 添加了 `srt_resilient_client_example` 示例目标

## 6. 使用建议

### 日志最佳实践
1. 在生产环境中，建议设置自定义日志回调，将日志输出到文件或日志系统
2. 使用文件位置信息帮助调试，但在生产环境可以选择性记录以减少日志大小
3. 根据需要过滤不同区域（Reactor/SRT）的日志

### 连接可靠性
1. 对于需要高可靠性的应用，参考 `srt_resilient_client_example` 实现自动重连
2. 设置合适的 SRT 选项（延迟、缓冲区大小等）以适应网络条件
3. 实现心跳机制以快速检测连接断开

### 性能优化
1. 使用协程进行异步操作，避免阻塞
2. 合理设置缓冲区大小和流控制参数
3. 在高负载场景下，考虑使用多个 acceptor 或负载均衡

## 7. 向后兼容性

### 破坏性变更
- `LogCallback` 签名变更，需要更新所有自定义日志回调的代码

### 迁移指南
```cpp
// 旧代码
SrtReactor::set_log_callback(
    [](LogLevel level, const char* area, const char* message) {
        // 处理日志
    }
);

// 新代码
SrtReactor::set_log_callback(
    [](LogLevel level, const char* area, const char* message,
       const char* file, const char* function, int line) {
        // 处理日志，可以选择是否使用新参数
    }
);
```

如果不需要文件位置信息，可以简单地忽略新参数。
