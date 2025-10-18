# SRT Socket 和 Acceptor 使用指南

本文档介绍如何使用 `asrt::SrtSocket` 和 `asrt::SrtAcceptor` 来构建基于 SRT 协议的客户端和服务器应用。

## 目录

- [概述](#概述)
- [SrtSocket - 客户端套接字](#srtsocket---客户端套接字)
- [SrtAcceptor - 服务器监听器](#srtacceptor---服务器监听器)
- [选项设置](#选项设置)
- [Packet 模式的读写](#packet-模式的读写)
- [错误处理](#错误处理)
- [日志配置](#日志配置)
- [完整示例](#完整示例)

## 概述

`asrt` 库提供了基于 ASIO 协程的 SRT 网络编程接口，包括：

- **SrtSocket**: 用于客户端连接和数据传输
- **SrtAcceptor**: 用于服务器端监听和接受连接
- **SrtReactor**: 底层 I/O 多路复用反应器（自动管理）

所有 I/O 操作都是异步的，基于 C++20 协程，使用 `co_await` 关键字。

## SrtSocket - 客户端套接字

### 创建和连接

```cpp
#include <asrt/srt_socket.hpp>
#include <asrt/srt_reactor.hpp>

asio::awaitable<void> connect_example() {
    auto& reactor = SrtReactor::get_instance();
    SrtSocket socket(reactor);
    
    // 连接到服务器
    co_await socket.async_connect("192.168.1.100", 9000);
    
    // 或者带超时
    co_await socket.async_connect("192.168.1.100", 9000, 
                                   std::chrono::milliseconds(5000));
}
```

### 设置连接回调

```cpp
socket.set_connect_callback([](const std::error_code& ec, SrtSocket& sock) {
    if (ec) {
        std::cerr << "Connection failed: " << ec.message() << std::endl;
    } else {
        std::cout << "Connected! Remote: " << sock.remote_address() << std::endl;
    }
});
```

### 读取数据包

SRT 支持消息模式（message API），每次读取一个完整的数据包：

```cpp
asio::awaitable<void> read_example(SrtSocket& socket) {
    char buffer[2048];
    
    // 读取一个数据包（无超时）
    size_t bytes = co_await socket.async_read_packet(buffer, sizeof(buffer));
    
    // 或者带超时（5秒）
    size_t bytes = co_await socket.async_read_packet(
        buffer, sizeof(buffer), 
        std::chrono::milliseconds(5000)
    );
    
    std::cout << "Received " << bytes << " bytes" << std::endl;
}
```

### 写入数据包

```cpp
asio::awaitable<void> write_example(SrtSocket& socket) {
    const char* message = "Hello, SRT!";
    
    // 写入一个数据包（无超时）
    size_t sent = co_await socket.async_write_packet(message, strlen(message));
    
    // 或者带超时（3秒）
    size_t sent = co_await socket.async_write_packet(
        message, strlen(message),
        std::chrono::milliseconds(3000)
    );
    
    std::cout << "Sent " << sent << " bytes" << std::endl;
}
```

### 查询状态

```cpp
// 检查连接状态
if (socket.status() == SRTSOCK_CONNECTED) {
    std::cout << "Socket is connected" << std::endl;
}

// 获取地址信息
std::cout << "Local: " << socket.local_address() << std::endl;
std::cout << "Remote: " << socket.remote_address() << std::endl;

// 获取统计信息
SRT_TRACEBSTATS stats;
if (socket.get_stats(stats)) {
    std::cout << "Packets sent: " << stats.pktSent << std::endl;
    std::cout << "RTT: " << stats.msRTT << " ms" << std::endl;
}
```

## SrtAcceptor - 服务器监听器

### 创建和绑定

```cpp
#include <asrt/srt_acceptor.hpp>

asio::awaitable<void> server_example() {
    auto& reactor = SrtReactor::get_instance();
    SrtAcceptor acceptor(reactor);
    
    // 绑定到端口（监听所有接口）
    acceptor.bind(9000);
    
    // 或指定地址和 backlog
    acceptor.bind("0.0.0.0", 9000, 10);
    
    std::cout << "Listening on: " << acceptor.local_address() << std::endl;
}
```

### 接受连接

```cpp
asio::awaitable<void> accept_example(SrtAcceptor& acceptor) {
    while (true) {
        // 接受一个连接（无超时）
        SrtSocket client = co_await acceptor.async_accept();
        
        // 或者带超时（10秒）
        SrtSocket client = co_await acceptor.async_accept(
            std::chrono::milliseconds(10000)
        );
        
        std::cout << "Accepted client from: " << client.remote_address() << std::endl;
        
        // 为每个客户端启动处理协程
        asio::co_spawn(
            reactor.get_io_context(),
            handle_client(std::move(client)),
            asio::detached
        );
    }
}
```

### 设置监听回调

```cpp
acceptor.set_listener_callback([](const std::error_code& ec, SrtSocket client) {
    if (ec) {
        std::cerr << "Accept error: " << ec.message() << std::endl;
    } else {
        std::cout << "New client: " << client.remote_address() << std::endl;
    }
});
```

## 选项设置

### 使用字符串设置单个选项

```cpp
socket.set_option("latency=200");           // 设置延迟为 200ms
socket.set_option("SRTO_RCVBUF=8388608");   // 8MB 接收缓冲区
socket.set_option("messageapi=1");          // 启用消息模式
```

### 批量设置选项

```cpp
std::map<std::string, std::string> options = {
    {"latency", "200"},           // 200ms 延迟
    {"rcvbuf", "8388608"},        // 8MB 接收缓冲区
    {"sndbuf", "8388608"},        // 8MB 发送缓冲区
    {"messageapi", "1"},          // 消息模式
    {"payloadsize", "1316"},      // 负载大小
    {"conntimeo", "5000"},        // 5秒连接超时
    {"passphrase", "mypassword"}, // 加密密码（可选）
    {"pbkeylen", "16"}            // 密钥长度（可选）
};

if (!socket.set_options(options)) {
    std::cerr << "Warning: Some options failed" << std::endl;
}
```

### 类型安全的选项设置

```cpp
socket.set_socket_option(SRTO_LATENCY, 200);
socket.set_socket_option(SRTO_MESSAGEAPI, true);
socket.set_socket_option(SRTO_PASSPHRASE, std::string("mypassword"));
```

### 常用选项说明

| 选项名称 | 类型 | 说明 |
|---------|------|------|
| `latency` | int | 延迟（毫秒），影响重传和播放延迟 |
| `rcvbuf` | int | 接收缓冲区大小（字节） |
| `sndbuf` | int | 发送缓冲区大小（字节） |
| `messageapi` | bool | 启用消息模式（推荐） |
| `payloadsize` | int | 负载大小，通常为 1316 |
| `conntimeo` | int | 连接超时（毫秒） |
| `maxbw` | int | 最大带宽（字节/秒），-1 表示无限制 |
| `passphrase` | string | 加密密码（10-79 字符） |
| `pbkeylen` | int | 密钥长度（16, 24, 32） |

## Packet 模式的读写

### 工作原理

1. **先尝试直接操作**：调用 `async_read_packet` 或 `async_write_packet` 时，会先尝试直接调用 SRT 的读写函数。
2. **遇到 EAGAIN 时等待**：如果返回 `EAGAIN`（would block），则自动调用 reactor 的 `async_wait_readable` 或 `async_wait_writable`。
3. **事件就绪后重试**：等待完成后，再次尝试读写操作。

这种设计避免了不必要的等待，提高了性能。

### 读取示例

```cpp
asio::awaitable<void> read_loop(SrtSocket& socket) {
    char buffer[2048];
    
    while (socket.is_open()) {
        try {
            // 读取一个数据包
            size_t bytes = co_await socket.async_read_packet(
                buffer, sizeof(buffer),
                std::chrono::milliseconds(5000)
            );
            
            // 处理数据
            process_data(buffer, bytes);
            
        } catch (const asio::system_error& e) {
            if (e.code() == std::errc::timed_out) {
                // 超时，检查连接状态
                if (socket.status() != SRTSOCK_CONNECTED) {
                    break;
                }
            } else {
                std::cerr << "Read error: " << e.what() << std::endl;
                break;
            }
        }
    }
}
```

### 写入示例

```cpp
asio::awaitable<void> write_loop(SrtSocket& socket) {
    for (int i = 0; i < 100; ++i) {
        std::string message = "Packet #" + std::to_string(i);
        
        try {
            size_t sent = co_await socket.async_write_packet(
                message.c_str(), message.size(),
                std::chrono::milliseconds(3000)
            );
            
            std::cout << "Sent packet " << i << ": " << sent << " bytes" << std::endl;
            
        } catch (const asio::system_error& e) {
            std::cerr << "Write error: " << e.what() << std::endl;
            break;
        }
        
        // 等待一会儿
        co_await asio::steady_timer(
            socket.get_io_context(), 
            std::chrono::milliseconds(100)
        ).async_wait(asio::use_awaitable);
    }
}
```

## 错误处理

### 异常类型

所有异步操作失败时会抛出 `asio::system_error` 异常，包含 `std::error_code`：

```cpp
try {
    co_await socket.async_connect("192.168.1.100", 9000);
} catch (const asio::system_error& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    std::cerr << "Code: " << e.code().value() << std::endl;
    std::cerr << "Category: " << e.code().category().name() << std::endl;
}
```

### 常见错误码

| 错误码 | 说明 |
|--------|------|
| `std::errc::timed_out` | 操作超时 |
| `std::errc::connection_refused` | 连接被拒绝 |
| `std::errc::connection_reset` | 连接被重置 |
| `std::errc::operation_would_block` | 操作会阻塞（通常不会传播到用户） |
| `asrt::srt_errc::connection_lost` | SRT 连接丢失 |
| `asrt::srt_errc::invalid_socket` | 无效的 socket |

### 回调中的错误处理

```cpp
socket.set_connect_callback([](const std::error_code& ec, SrtSocket& sock) {
    if (ec) {
        if (ec == std::errc::connection_refused) {
            std::cerr << "Server refused connection" << std::endl;
        } else if (ec == std::errc::timed_out) {
            std::cerr << "Connection timeout" << std::endl;
        } else {
            std::cerr << "Connection error: " << ec.message() << std::endl;
        }
    } else {
        std::cout << "Connected successfully!" << std::endl;
    }
});
```

## 日志配置

### 设置日志级别

```cpp
// 设置全局日志级别
SrtReactor::set_log_level(LogLevel::Debug);    // 详细调试信息
SrtReactor::set_log_level(LogLevel::Notice);   // 一般信息（默认）
SrtReactor::set_log_level(LogLevel::Warning);  // 仅警告
SrtReactor::set_log_level(LogLevel::Error);    // 仅错误
```

### 自定义日志回调

```cpp
SrtReactor::set_log_callback([](LogLevel level, const char* area, const char* message) {
    // area: "Reactor" 或 "SRT" 等
    // message: 日志内容
    
    const char* level_str = "";
    switch (level) {
        case LogLevel::Debug:    level_str = "[DEBUG]"; break;
        case LogLevel::Notice:   level_str = "[INFO ]"; break;
        case LogLevel::Warning:  level_str = "[WARN ]"; break;
        case LogLevel::Error:    level_str = "[ERROR]"; break;
        case LogLevel::Critical: level_str = "[FATAL]"; break;
    }
    
    // 输出到文件、syslog 或其他日志系统
    std::cout << level_str << " [" << area << "] " << message << std::endl;
});
```

### 恢复默认日志

```cpp
// 传入 nullptr 恢复默认输出（stderr）
SrtReactor::set_log_callback(nullptr);
```

## 完整示例

### 服务器示例

```cpp
#include <asrt/srt_acceptor.hpp>
#include <asrt/srt_socket.hpp>
#include <asrt/srt_reactor.hpp>

asio::awaitable<void> handle_client(SrtSocket client) {
    char buffer[2048];
    
    while (client.is_open()) {
        try {
            size_t bytes = co_await client.async_read_packet(
                buffer, sizeof(buffer), std::chrono::seconds(5)
            );
            
            // 回显
            co_await client.async_write_packet(buffer, bytes);
            
        } catch (const asio::system_error& e) {
            if (e.code() != std::errc::timed_out) {
                std::cerr << "Error: " << e.what() << std::endl;
                break;
            }
        }
    }
}

asio::awaitable<void> run_server() {
    auto& reactor = SrtReactor::get_instance();
    SrtAcceptor acceptor(reactor);
    
    acceptor.set_options({
        {"latency", "200"},
        {"messageapi", "1"}
    });
    
    acceptor.bind(9000);
    std::cout << "Listening on " << acceptor.local_address() << std::endl;
    
    while (true) {
        SrtSocket client = co_await acceptor.async_accept();
        
        asio::co_spawn(
            reactor.get_io_context(),
            handle_client(std::move(client)),
            asio::detached
        );
    }
}

int main() {
    auto& reactor = SrtReactor::get_instance();
    
    asio::co_spawn(
        reactor.get_io_context(),
        run_server(),
        asio::detached
    );
    
    std::this_thread::sleep_for(std::chrono::hours(24));
    return 0;
}
```

### 客户端示例

```cpp
#include <asrt/srt_socket.hpp>
#include <asrt/srt_reactor.hpp>

asio::awaitable<void> run_client() {
    auto& reactor = SrtReactor::get_instance();
    SrtSocket socket(reactor);
    
    socket.set_options({
        {"latency", "200"},
        {"messageapi", "1"}
    });
    
    co_await socket.async_connect("127.0.0.1", 9000, std::chrono::seconds(5));
    std::cout << "Connected!" << std::endl;
    
    for (int i = 0; i < 10; ++i) {
        std::string msg = "Hello #" + std::to_string(i);
        
        co_await socket.async_write_packet(msg.c_str(), msg.size());
        
        char buffer[2048];
        size_t bytes = co_await socket.async_read_packet(buffer, sizeof(buffer));
        
        std::cout << "Received: " << std::string(buffer, bytes) << std::endl;
        
        co_await asio::steady_timer(reactor.get_io_context(), std::chrono::seconds(1))
            .async_wait(asio::use_awaitable);
    }
}

int main() {
    auto& reactor = SrtReactor::get_instance();
    
    asio::co_spawn(
        reactor.get_io_context(),
        run_client(),
        asio::detached
    );
    
    std::this_thread::sleep_for(std::chrono::seconds(30));
    return 0;
}
```

## 性能优化建议

1. **合理设置缓冲区大小**：根据网络带宽和延迟调整 `rcvbuf` 和 `sndbuf`。
2. **调整延迟参数**：低延迟场景使用 50-200ms，直播场景可以使用更大值。
3. **使用消息模式**：对于数据包传输，建议启用 `messageapi=1`。
4. **批量发送**：如果有多个小数据包，考虑合并后发送。
5. **监控统计信息**：定期调用 `get_stats()` 监控网络状况。

## 故障排查

### 连接失败

- 检查防火墙是否允许 UDP 流量
- 确认服务器正在监听正确的端口
- 检查 `conntimeo` 设置是否合理

### 数据传输慢

- 检查 `latency` 设置是否过大
- 使用 `get_stats()` 查看丢包率和 RTT
- 调整 `maxbw` 限制

### 日志过多

- 降低日志级别：`SrtReactor::set_log_level(LogLevel::Warning)`
- 或者完全禁用调试日志

## 参考资源

- [SRT 官方文档](https://github.com/Haivision/srt)
- [ASIO 文档](https://think-async.com/Asio/)
- 项目示例：`examples/srt_server_example.cpp` 和 `examples/srt_client_example.cpp`

