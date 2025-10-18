# 更新总结：SRT 回调和选项说明

## 更新内容

本次更新添加了关于 SRT 原生回调接口和选项设置时机的详细文档。

## 新增文档

### 1. SRT 原生回调接口说明
**文件**: `docs/SRT_NATIVE_CALLBACKS.md`

说明了：
- SRT 的 C 风格原生回调接口（`srt_listen_callback` 和 `srt_connect_callback`）
- 当前 C++ 封装与原生接口的关系
- Socket 选项的三种设置时机：pre-bind、pre、post
- 各类选项的详细列表和使用建议

### 2. 回调和选项使用指南
**文件**: `CALLBACK_AND_OPTIONS_GUIDE.md`

提供了：
- 完整的选项分类和说明表格
- 四种典型应用场景的配置示例
  - 直播服务器（高吞吐量）
  - 文件传输（高可靠性）
  - 加密传输
  - 多客户端差异化服务
- 选项设置的最佳实践
- 常见问题解答

### 3. 高级服务器示例
**文件**: `examples/srt_advanced_server.cpp`

展示了：
- pre-bind 和 pre 选项的正确设置时机
- post 选项的动态调整
- 使用 listener callback 实现差异化服务
- 详细的连接统计和监控
- 会话管理和资源追踪

## 关键要点

### SRT 选项设置时机

1. **pre-bind 选项**（必须在 `srt_bind` 前设置）
   - 影响底层 UDP socket
   - 如：MSS、缓冲区大小、UDP 选项

2. **pre 选项**（必须在连接前设置）
   - 影响连接协商
   - 如：延迟、加密、传输类型

3. **post 选项**（任何时候都可以设置）
   - 运行时行为控制
   - 如：带宽限制、超时设置

### 当前实现说明

当前的 `SrtSocket` 和 `SrtAcceptor` 实现：

✅ **已支持**：
- 三种方式设置选项（字符串、Map、类型安全）
- 在连接/绑定前自动应用所有选项
- C++ 风格的回调封装
- 完善的错误处理和日志

⚠️ **实现细节**：
- 所有选项在连接/绑定前一次性应用
- 对大多数场景是安全的
- 如果违反时机限制，选项设置会失败但不抛出异常

💡 **使用建议**：
- 在创建 socket 后立即设置所有需要的选项
- 使用 Map 批量设置可以确保顺序
- 利用 listener callback 实现动态配置

## 实际使用示例

### 基本使用

```cpp
// 服务器端
SrtAcceptor acceptor(reactor);

// 一次性设置所有选项
acceptor.set_options({
    // pre-bind
    {"mss", "1500"},
    {"rcvbuf", "16777216"},
    // pre
    {"latency", "200"},
    {"messageapi", "1"}
});

acceptor.bind(9000);
```

### 高级使用

```cpp
// 分阶段设置
SrtAcceptor acceptor(reactor);

// 阶段 1: pre-bind 选项
acceptor.set_options({
    {"mss", "1500"},
    {"rcvbuf", "16777216"},
    {"sndbuf", "16777216"}
});

// 阶段 2: pre 选项
acceptor.set_options({
    {"latency", "200"},
    {"messageapi", "1"},
    {"fc", "32768"}
});

// 阶段 3: 绑定
acceptor.bind(9000);

// 阶段 4: 设置回调
acceptor.set_listener_callback([](const std::error_code& ec, SrtSocket client) {
    if (!ec) {
        // 根据客户端动态设置 post 选项
        client.set_option("maxbw=10000000");
    }
});
```

## 编译和运行新示例

```bash
# 编译
cd /home/ubuntu/codes/cpp/asio_srt
cmake -B build
cmake --build build -j4

# 运行高级服务器示例
./build/examples/srt_advanced_server 9000

# 运行客户端（另一个终端）
./build/examples/srt_client_example 127.0.0.1 9000
```

## 文档结构

```
/home/ubuntu/codes/cpp/asio_srt/
├── docs/
│   ├── SRT_SOCKET_GUIDE.md           # 完整 API 文档
│   └── SRT_NATIVE_CALLBACKS.md       # 原生回调接口说明 [NEW]
├── examples/
│   ├── srt_server_example.cpp        # 基本服务器
│   ├── srt_client_example.cpp        # 基本客户端
│   └── srt_advanced_server.cpp       # 高级服务器示例 [NEW]
├── QUICK_START.md                     # 快速入门
├── SRT_IMPLEMENTATION_SUMMARY.md      # 实现总结
├── CALLBACK_AND_OPTIONS_GUIDE.md      # 回调和选项指南 [NEW]
└── NEW_FEATURES.md                    # 新功能说明
```

## 后续改进建议

如果需要更精细的控制，可以考虑：

1. **扩展选项类**：
   - 将选项按时机分组（pre-bind、pre、post）
   - 在设置时自动验证时机

2. **暴露原生回调**：
   - 允许用户注册 SRT 原生回调
   - 提供更底层的控制能力

3. **Stream ID 支持**：
   - 添加获取和解析 Stream ID 的接口
   - 实现基于 Stream ID 的访问控制

4. **选项验证**：
   - 在设置前验证选项值
   - 检查选项之间的依赖关系

5. **动态选项查询**：
   - 添加 `get_option` 接口
   - 允许读取当前选项值

## 参考资源

- [SRT 官方文档](https://github.com/Haivision/srt)
- [SRT API 文档](https://github.com/Haivision/srt/blob/master/docs/API/API.md)
- [SRT Socket 选项](https://github.com/Haivision/srt/blob/master/docs/API/API-socket-options.md)

## 总结

本次更新主要是文档和示例的补充，帮助用户：

1. ✅ 理解 SRT 原生回调接口的工作原理
2. ✅ 掌握 Socket 选项的正确设置时机
3. ✅ 学习实际应用场景的配置方法
4. ✅ 通过高级示例了解最佳实践

核心实现（`SrtSocket` 和 `SrtAcceptor`）保持不变，仍然功能完整且可以正常使用。文档的补充使得用户能够更好地理解和使用这些功能。

