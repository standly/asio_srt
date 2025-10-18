# SRT Socket 快速入门

## 快速开始

### 编译项目

```bash
cd /home/ubuntu/codes/cpp/asio_srt
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
```

### 运行示例

#### 1. 启动服务器

在一个终端窗口运行：

```bash
./build/examples/srt_server_example 9000
```

输出示例：
```
=== SRT Server Example ===
Port: 9000

[INFO ] [Reactor] SrtReactor initializing...
[INFO ] [Reactor] Listening on 0.0.0.0:9000 (fd=8, backlog=5)
Server listening on: 0.0.0.0:9000
Waiting for connections...
Press Ctrl+C to stop the server...
```

#### 2. 运行客户端

在另一个终端窗口运行：

```bash
./build/examples/srt_client_example 127.0.0.1 9000
```

输出示例：
```
=== SRT Client Example ===
Server: 127.0.0.1:9000

[INFO ] [Reactor] Connecting to 127.0.0.1:9000 (fd=10)
Connected successfully!
Local address: 127.0.0.1:34567
Remote address: 127.0.0.1:9000

Sending: Hello from client, message #1
Sent 32 bytes
Received 32 bytes: Hello from client, message #1
...
```

## 5 分钟教程

### 编写一个简单的回显服务器

```cpp
#include <asrt/srt_acceptor.hpp>
#include <asrt/srt_socket.hpp>
#include <asrt/srt_reactor.hpp>
#include <iostream>

// 处理客户端的协程
asio::awaitable<void> handle_client(asrt::SrtSocket client) {
    char buffer[2048];
    
    while (client.is_open()) {
        try {
            // 读取数据包
            size_t bytes = co_await client.async_read_packet(
                buffer, sizeof(buffer), 
                std::chrono::seconds(30)
            );
            
            std::cout << "Received " << bytes << " bytes" << std::endl;
            
            // 回显
            co_await client.async_write_packet(buffer, bytes);
            
        } catch (const asio::system_error& e) {
            std::cerr << "Error: " << e.what() << std::endl;
            break;
        }
    }
}

// 服务器主循环
asio::awaitable<void> run_server() {
    auto& reactor = asrt::SrtReactor::get_instance();
    asrt::SrtAcceptor acceptor(reactor);
    
    // 设置选项
    acceptor.set_options({
        {"latency", "200"},
        {"messageapi", "1"}
    });
    
    // 监听端口
    acceptor.bind(9000);
    std::cout << "Listening on " << acceptor.local_address() << std::endl;
    
    // 接受连接循环
    while (true) {
        asrt::SrtSocket client = co_await acceptor.async_accept();
        std::cout << "New client: " << client.remote_address() << std::endl;
        
        // 为每个客户端启动处理协程
        asio::co_spawn(
            reactor.get_io_context(),
            handle_client(std::move(client)),
            asio::detached
        );
    }
}

int main() {
    auto& reactor = asrt::SrtReactor::get_instance();
    
    // 设置日志级别
    asrt::SrtReactor::set_log_level(asrt::LogLevel::Notice);
    
    // 启动服务器
    asio::co_spawn(
        reactor.get_io_context(),
        run_server(),
        asio::detached
    );
    
    // 保持运行
    std::this_thread::sleep_for(std::chrono::hours(24));
    return 0;
}
```

### 编写一个简单的客户端

```cpp
#include <asrt/srt_socket.hpp>
#include <asrt/srt_reactor.hpp>
#include <iostream>
#include <string>

asio::awaitable<void> run_client() {
    auto& reactor = asrt::SrtReactor::get_instance();
    asrt::SrtSocket socket(reactor);
    
    // 设置选项
    socket.set_options({
        {"latency", "200"},
        {"messageapi", "1"}
    });
    
    // 连接到服务器
    co_await socket.async_connect("127.0.0.1", 9000);
    std::cout << "Connected to " << socket.remote_address() << std::endl;
    
    // 发送消息
    std::string message = "Hello, SRT!";
    co_await socket.async_write_packet(message.c_str(), message.size());
    std::cout << "Sent: " << message << std::endl;
    
    // 接收回显
    char buffer[2048];
    size_t bytes = co_await socket.async_read_packet(buffer, sizeof(buffer));
    std::cout << "Received: " << std::string(buffer, bytes) << std::endl;
}

int main() {
    auto& reactor = asrt::SrtReactor::get_instance();
    
    asio::co_spawn(
        reactor.get_io_context(),
        run_client(),
        asio::detached
    );
    
    std::this_thread::sleep_for(std::chrono::seconds(10));
    return 0;
}
```

## 常用代码片段

### 设置选项

```cpp
// 方式 1：使用 map
std::map<std::string, std::string> options = {
    {"latency", "200"},
    {"rcvbuf", "8388608"},
    {"messageapi", "1"}
};
socket.set_options(options);

// 方式 2：单个设置
socket.set_option("latency=200");
socket.set_option("messageapi=1");

// 方式 3：类型安全
socket.set_socket_option(SRTO_LATENCY, 200);
socket.set_socket_option(SRTO_MESSAGEAPI, true);
```

### 带超时的操作

```cpp
// 连接超时
co_await socket.async_connect("192.168.1.100", 9000, 
                               std::chrono::seconds(5));

// 读超时
size_t bytes = co_await socket.async_read_packet(
    buffer, sizeof(buffer),
    std::chrono::seconds(10)
);

// 写超时
co_await socket.async_write_packet(
    data, size,
    std::chrono::seconds(3)
);
```

### 错误处理

```cpp
try {
    co_await socket.async_connect("192.168.1.100", 9000);
} catch (const asio::system_error& e) {
    if (e.code() == std::errc::timed_out) {
        std::cerr << "Connection timeout" << std::endl;
    } else if (e.code() == std::errc::connection_refused) {
        std::cerr << "Connection refused" << std::endl;
    } else {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}
```

### 使用回调

```cpp
// 连接回调
socket.set_connect_callback([](const std::error_code& ec, asrt::SrtSocket& sock) {
    if (!ec) {
        std::cout << "Connected to " << sock.remote_address() << std::endl;
    }
});

// 监听回调
acceptor.set_listener_callback([](const std::error_code& ec, asrt::SrtSocket client) {
    if (!ec) {
        std::cout << "New client from " << client.remote_address() << std::endl;
    }
});
```

### 自定义日志

```cpp
// 设置日志级别
asrt::SrtReactor::set_log_level(asrt::LogLevel::Debug);

// 自定义日志回调
asrt::SrtReactor::set_log_callback([](asrt::LogLevel level, const char* area, const char* message) {
    // 输出到文件、数据库等
    std::cout << "[" << area << "] " << message << std::endl;
});
```

### 获取统计信息

```cpp
SRT_TRACEBSTATS stats;
if (socket.get_stats(stats)) {
    std::cout << "Packets sent: " << stats.pktSent << std::endl;
    std::cout << "Packets received: " << stats.pktRecv << std::endl;
    std::cout << "Packets lost: " << stats.pktSndLoss << std::endl;
    std::cout << "RTT: " << stats.msRTT << " ms" << std::endl;
    std::cout << "Bandwidth: " << stats.mbpsSendRate << " Mbps" << std::endl;
}
```

## 调试技巧

### 启用详细日志

```cpp
asrt::SrtReactor::set_log_level(asrt::LogLevel::Debug);
```

### 检查连接状态

```cpp
if (socket.status() == SRTS_CONNECTED) {
    std::cout << "Connected" << std::endl;
} else if (socket.status() == SRTS_BROKEN) {
    std::cout << "Connection broken" << std::endl;
}
```

### 查看地址信息

```cpp
std::cout << "Local: " << socket.local_address() << std::endl;
std::cout << "Remote: " << socket.remote_address() << std::endl;
```

## 常见问题

### Q: 连接超时怎么办？

A: 增加连接超时时间或检查网络连接：
```cpp
socket.set_option("conntimeo=10000");  // 10秒
co_await socket.async_connect(host, port, std::chrono::seconds(10));
```

### Q: 如何提高吞吐量？

A: 增加缓冲区大小和调整延迟参数：
```cpp
socket.set_options({
    {"rcvbuf", "16777216"},    // 16MB
    {"sndbuf", "16777216"},    // 16MB
    {"latency", "120"},        // 减少延迟
    {"maxbw", "-1"}            // 无带宽限制
});
```

### Q: 如何启用加密？

A: 设置密码和密钥长度：
```cpp
socket.set_options({
    {"passphrase", "mySecretPassword123"},
    {"pbkeylen", "32"}  // 256-bit 加密
});
```

### Q: 读写超时如何处理？

A: 捕获超时异常并检查连接状态：
```cpp
try {
    size_t bytes = co_await socket.async_read_packet(
        buffer, sizeof(buffer), std::chrono::seconds(5)
    );
} catch (const asio::system_error& e) {
    if (e.code() == std::errc::timed_out) {
        // 检查连接是否还活着
        if (socket.status() != SRTS_CONNECTED) {
            // 连接已断开
            break;
        }
        // 连接正常，只是暂时没数据
        continue;
    }
}
```

## 性能优化建议

1. **使用消息模式**：`messageapi=1`（推荐）
2. **调整缓冲区**：根据网络带宽设置合适的缓冲区大小
3. **优化延迟参数**：低延迟场景使用 50-120ms，直播场景可以更大
4. **批量发送**：将多个小包合并成大包发送
5. **使用连接池**：复用连接减少建立开销

## 下一步

- 查看 [完整文档](docs/SRT_SOCKET_GUIDE.md)
- 阅读 [实现总结](SRT_IMPLEMENTATION_SUMMARY.md)
- 研究 [示例代码](examples/)
- 参考 [SRT 官方文档](https://github.com/Haivision/srt)

## 需要帮助？

如果遇到问题，可以：
1. 启用 Debug 日志查看详细信息
2. 检查 SRT 错误码和日志
3. 参考示例代码
4. 查阅 SRT 官方文档

