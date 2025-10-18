# SRT Socket/Acceptor V2 实现总结

## 完成的改进

### 1. 原生 SRT 回调支持 ✅

#### 连接回调
- 使用 `srt_connect_callback` 原生 API
- 在 SRT 协议栈层面触发
- 支持异步通知连接状态

#### 监听回调  
- 使用 `srt_listen_callback` 原生 API
- 在握手阶段调用，支持访问控制
- 新的回调签名：`int callback(SrtSocket&, int hsversion, const std::string& streamid)`
- 返回 0 接受连接，-1 拒绝连接

### 2. 智能选项管理 ✅

#### 选项时机分类
```cpp
enum class OptionTiming {
    PRE_BIND,   // bind 之前必须设置
    PRE,        // 连接之前必须设置  
    POST        // 任何时候都可以设置
};
```

#### 自动时机管理
- 构造函数支持选项 map：`SrtSocket(options_map)`
- 系统自动在正确时机应用选项
- 详细的选项分类映射表

### 3. 增强功能 ✅

- 完善的错误处理和日志
- 支持彩色日志输出
- 基于 Stream ID 的访问控制
- 连接配置文件（profile）支持

## 代码变更

### 新增文件
- `src/asrt/srt_socket_options.hpp/cpp` - 智能选项管理器
- `examples/srt_server_v2_example.cpp` - 展示新功能的服务器示例
- `examples/srt_client_v2_example.cpp` - 展示新功能的客户端示例
- `docs/SRT_V2_FEATURES.md` - 新功能详细说明

### 修改文件
- `src/asrt/srt_socket.hpp/cpp` - 支持原生回调和智能选项
- `src/asrt/srt_acceptor.hpp/cpp` - 支持原生监听回调
- `src/asrt/CMakeLists.txt` - 包含新文件
- `examples/CMakeLists.txt` - 构建新示例

## 使用示例

### 服务器端
```cpp
// 创建带预配置选项的 acceptor
std::map<std::string, std::string> options = {
    {"latency", "120"},
    {"messageapi", "1"},
    {"rcvbuf", "8388608"}
};
SrtAcceptor acceptor(options);

// 设置访问控制回调
acceptor.set_listener_callback([](SrtSocket& client, int hsversion, const std::string& streamid) -> int {
    if (streamid == "reject_me") return -1;
    if (streamid == "low_latency") {
        client.set_option("rcvlatency=50");
    }
    return 0;
});

acceptor.bind(9000);
```

### 客户端
```cpp
// 创建带选项的 socket
std::map<std::string, std::string> options = {
    {"streamid", "myapp"},
    {"latency", "50"}
};
SrtSocket socket(options);

// 设置连接通知
socket.set_connect_callback([](const std::error_code& ec, SrtSocket& sock) {
    if (!ec) {
        std::cout << "Connected to " << sock.remote_address() << std::endl;
    }
});

co_await socket.async_connect("server", 9000);
```

## 测试方法

1. **编译项目**
   ```bash
   cd /home/ubuntu/codes/cpp/asio_srt
   cmake --build build
   ```

2. **运行 V2 服务器**
   ```bash
   ./build/examples/srt_server_v2_example 9000
   ```

3. **运行 V2 客户端**
   ```bash
   # 默认配置
   ./build/examples/srt_client_v2_example localhost 9000
   
   # 低延迟配置
   ./build/examples/srt_client_v2_example -p low_latency -s mystream localhost 9000
   
   # 测试拒绝连接
   ./build/examples/srt_client_v2_example -s reject localhost 9000
   ```

## 关键优势

1. **更可靠的回调机制**：使用 SRT 原生 API，在协议栈层面触发
2. **简化的选项管理**：自动处理复杂的时机要求
3. **灵活的访问控制**：基于 Stream ID 的细粒度控制
4. **更好的调试体验**：彩色日志，详细的状态信息

## 注意事项

1. 回调在 SRT 线程中执行，应避免长时间阻塞
2. 选项值需要符合 SRT 的要求（范围、类型等）
3. 某些选项仅在特定 SRT 版本可用（已做兼容处理）

## 后续改进建议

1. 支持 IPv6
2. 添加组播支持
3. 实现连接池管理
4. 添加性能基准测试
