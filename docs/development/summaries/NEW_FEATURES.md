# 🎉 新增功能：SRT Socket 和 Acceptor

## 重要更新

本次更新新增了完整的 **SRT Socket** 和 **SRT Acceptor** 实现，现在可以轻松构建 SRT 客户端和服务器应用！

## ✨ 新增组件

### 1. SrtSocket - 客户端 Socket

完整的 SRT 客户端 socket 实现，支持：

- ✅ **异步连接**：基于协程的异步连接，支持超时
- ✅ **Packet 读写**：基于消息模式的数据包读写
- ✅ **灵活的选项设置**：支持字符串、Map、类型安全三种方式
- ✅ **连接回调**：在连接建立/失败时自动调用
- ✅ **统计信息**：获取详细的连接统计数据
- ✅ **完善的日志**：详细的操作日志和错误信息

**快速示例**：
```cpp
auto& reactor = SrtReactor::get_instance();
SrtSocket socket(reactor);

// 设置选项
socket.set_options({
    {"latency", "200"},
    {"messageapi", "1"}
});

// 连接
co_await socket.async_connect("192.168.1.100", 9000);

// 发送数据包
co_await socket.async_write_packet(data, size);

// 接收数据包
size_t bytes = co_await socket.async_read_packet(buffer, sizeof(buffer));
```

### 2. SrtAcceptor - 服务器监听器

完整的 SRT 服务器监听器实现，支持：

- ✅ **异步接受连接**：协程友好的连接接受
- ✅ **灵活的绑定**：支持指定地址和端口
- ✅ **监听回调**：在接受新连接时自动调用
- ✅ **选项配置**：与 SrtSocket 相同的选项设置机制
- ✅ **超时支持**：支持带超时的接受操作

**快速示例**：
```cpp
auto& reactor = SrtReactor::get_instance();
SrtAcceptor acceptor(reactor);

// 绑定端口
acceptor.bind(9000);

// 接受连接
while (true) {
    SrtSocket client = co_await acceptor.async_accept();
    
    // 为每个客户端启动处理协程
    asio::co_spawn(reactor.get_io_context(),
                   handle_client(std::move(client)),
                   asio::detached);
}
```

## 🚀 核心特性

### 1. 智能的 I/O 处理

读写操作采用**先尝试后等待**的策略：

1. **先尝试直接读写**：调用 SRT 的读写函数
2. **EAGAIN 时自动等待**：如果会阻塞，自动调用 reactor 的 wait 函数
3. **事件就绪后重试**：等待完成后重新尝试操作

这种设计避免了不必要的等待，最大化性能！

### 2. 灵活的选项配置

支持三种配置方式，随你选择：

```cpp
// 方式 1：字符串格式
socket.set_option("latency=200");
socket.set_option("messageapi=1");

// 方式 2：Map 批量设置
socket.set_options({
    {"latency", "200"},
    {"rcvbuf", "8388608"},
    {"messageapi", "1"}
});

// 方式 3：类型安全
socket.set_socket_option(SRTO_LATENCY, 200);
socket.set_socket_option(SRTO_MESSAGEAPI, true);
```

支持 30+ 常用 SRT 选项，包括延迟、缓冲区、加密等。

### 3. 完善的错误处理

- **标准错误码**：使用 `std::error_code`，与 ASIO 完全兼容
- **详细的日志**：每个操作都有详细的日志记录
- **异常传播**：协程友好的异常处理
- **错误回调**：在回调中获取错误信息

### 4. 回调支持

为异步操作设置回调：

```cpp
// 连接回调
socket.set_connect_callback([](const std::error_code& ec, SrtSocket& sock) {
    if (!ec) {
        std::cout << "Connected to " << sock.remote_address() << std::endl;
    }
});

// 监听回调
acceptor.set_listener_callback([](const std::error_code& ec, SrtSocket client) {
    if (!ec) {
        std::cout << "New client: " << client.remote_address() << std::endl;
    }
});
```

### 5. 增强的日志系统

完善的日志输出，便于调试和问题排查：

```cpp
// 设置日志级别
SrtReactor::set_log_level(LogLevel::Debug);

// 自定义日志回调
SrtReactor::set_log_callback([](LogLevel level, const char* area, const char* message) {
    // 输出到你的日志系统
    my_logger.log(level, area, message);
});
```

日志覆盖：
- Socket 生命周期
- 连接建立过程
- 数据读写操作
- 错误和异常
- Epoll 事件

## 📚 新增文档

1. **[SRT Socket 使用指南](docs/SRT_SOCKET_GUIDE.md)** - 详细的 API 文档和示例
2. **[快速入门](QUICK_START.md)** - 5 分钟上手教程
3. **[实现总结](SRT_IMPLEMENTATION_SUMMARY.md)** - 技术实现细节

## 🎯 示例程序

新增两个完整的示例程序：

### 服务器示例
```bash
./build/examples/srt_server_example 9000
```

功能：
- 监听指定端口
- 接受多个客户端连接
- 回显客户端发送的数据包
- 完整的日志输出

### 客户端示例
```bash
./build/examples/srt_client_example 127.0.0.1 9000
```

功能：
- 连接到服务器
- 发送测试数据包
- 接收服务器回显
- 显示连接统计信息

## 🔧 编译和运行

### 编译项目
```bash
cd /home/ubuntu/codes/cpp/asio_srt
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
```

### 运行测试
```bash
# 终端 1：启动服务器
./build/examples/srt_server_example 9000

# 终端 2：运行客户端
./build/examples/srt_client_example 127.0.0.1 9000
```

## 📊 新增文件

```
src/asrt/
├── srt_socket.hpp         # SRT Socket 头文件
├── srt_socket.cpp         # SRT Socket 实现
├── srt_acceptor.hpp       # SRT Acceptor 头文件
└── srt_acceptor.cpp       # SRT Acceptor 实现

examples/
├── srt_server_example.cpp # 服务器示例
└── srt_client_example.cpp # 客户端示例

docs/
└── SRT_SOCKET_GUIDE.md    # 详细使用指南
```

## 🎓 使用示例

### 完整的服务器
```cpp
#include <asrt/srt_acceptor.hpp>
#include <asrt/srt_socket.hpp>
#include <asrt/srt_reactor.hpp>

asio::awaitable<void> handle_client(asrt::SrtSocket client) {
    char buffer[2048];
    while (client.is_open()) {
        try {
            size_t bytes = co_await client.async_read_packet(buffer, sizeof(buffer));
            co_await client.async_write_packet(buffer, bytes);
        } catch (const asio::system_error& e) {
            std::cerr << "Error: " << e.what() << std::endl;
            break;
        }
    }
}

asio::awaitable<void> run_server() {
    auto& reactor = asrt::SrtReactor::get_instance();
    asrt::SrtAcceptor acceptor(reactor);
    
    acceptor.set_options({{"latency", "200"}, {"messageapi", "1"}});
    acceptor.bind(9000);
    
    while (true) {
        asrt::SrtSocket client = co_await acceptor.async_accept();
        asio::co_spawn(reactor.get_io_context(),
                       handle_client(std::move(client)),
                       asio::detached);
    }
}

int main() {
    auto& reactor = asrt::SrtReactor::get_instance();
    asio::co_spawn(reactor.get_io_context(), run_server(), asio::detached);
    std::this_thread::sleep_for(std::chrono::hours(24));
    return 0;
}
```

### 完整的客户端
```cpp
#include <asrt/srt_socket.hpp>
#include <asrt/srt_reactor.hpp>

asio::awaitable<void> run_client() {
    auto& reactor = asrt::SrtReactor::get_instance();
    asrt::SrtSocket socket(reactor);
    
    socket.set_options({{"latency", "200"}, {"messageapi", "1"}});
    
    co_await socket.async_connect("127.0.0.1", 9000);
    
    std::string message = "Hello, SRT!";
    co_await socket.async_write_packet(message.c_str(), message.size());
    
    char buffer[2048];
    size_t bytes = co_await socket.async_read_packet(buffer, sizeof(buffer));
    std::cout << "Received: " << std::string(buffer, bytes) << std::endl;
}

int main() {
    auto& reactor = asrt::SrtReactor::get_instance();
    asio::co_spawn(reactor.get_io_context(), run_client(), asio::detached);
    std::this_thread::sleep_for(std::chrono::seconds(10));
    return 0;
}
```

## 🔍 技术亮点

1. **零拷贝设计** - 直接使用用户缓冲区
2. **协程友好** - 完全基于 C++20 协程
3. **高性能** - 先尝试直接 I/O，避免不必要等待
4. **类型安全** - 使用标准错误码和强类型
5. **易于使用** - 清晰的 API 设计

## 🐛 已知限制

1. 当前主要测试 IPv4，IPv6 已支持但未充分测试
2. 移动赋值不会改变 reactor 引用
3. 需要 OpenSSL 库支持加密功能

## 📝 后续计划

- [ ] 添加连接池支持
- [ ] 实现流式 API
- [ ] 增强统计监控
- [ ] 添加性能基准测试
- [ ] 更多示例程序

## 🤝 贡献

欢迎贡献代码、报告问题或提出建议！

## 📖 更多资源

- [SRT Socket 使用指南](docs/SRT_SOCKET_GUIDE.md) - 完整 API 文档
- [快速入门](QUICK_START.md) - 快速上手教程
- [实现总结](SRT_IMPLEMENTATION_SUMMARY.md) - 技术实现细节
- [SRT 官方文档](https://github.com/Haivision/srt) - SRT 协议文档

---

**现在就开始使用 SRT Socket 和 Acceptor 构建你的实时传输应用吧！** 🚀

