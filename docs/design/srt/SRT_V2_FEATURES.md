# SRT Socket 和 Acceptor V2 功能说明

## 概述

本次更新主要解决了两个核心问题：

1. **原生 SRT 回调支持**：通过 SRT 协议栈原生接口设置回调，而不是应用层模拟
2. **智能选项管理**：自动识别并在正确的时机应用 SRT 选项

## 主要改进

### 1. 原生 SRT 回调机制

#### 连接回调 (Connect Callback)

```cpp
// 设置连接回调
socket.set_connect_callback([](const std::error_code& ec, SrtSocket& sock) {
    if (ec) {
        std::cerr << "连接失败: " << ec.message() << std::endl;
    } else {
        std::cout << "连接成功!" << std::endl;
        std::cout << "本地地址: " << sock.local_address() << std::endl;
        std::cout << "远程地址: " << sock.remote_address() << std::endl;
    }
});
```

特点：
- 使用 `srt_connect_callback` 原生 API 设置
- 在 SRT 协议栈层面触发，更准确可靠
- 支持获取连接状态和统计信息

#### 监听回调 (Listener Callback)

```cpp
// 设置监听回调（新签名）
acceptor.set_listener_callback([](SrtSocket& client, int hsversion, const std::string& streamid) -> int {
    std::cout << "新连接请求" << std::endl;
    std::cout << "握手版本: " << hsversion << std::endl;
    std::cout << "Stream ID: " << streamid << std::endl;
    
    // 基于条件决定是否接受连接
    if (streamid.find("reject") != std::string::npos) {
        return -1;  // 拒绝连接
    }
    
    // 为特定客户端设置选项
    if (streamid == "low_latency") {
        client.set_option("rcvlatency=50");
    }
    
    return 0;  // 接受连接
});
```

特点：
- 使用 `srt_listen_callback` 原生 API 设置
- 在握手阶段调用，可以实现访问控制
- 可以为每个连接设置不同的选项
- 返回值控制是否接受连接

### 2. 智能选项管理

#### 选项时机分类

SRT 选项分为三个时机：

1. **Pre-bind 选项**：必须在 bind 之前设置
   - MSS、UDP 缓冲区大小、IP 选项等
   
2. **Pre 选项**：必须在连接之前设置
   - 延迟、传输模式、加密等
   
3. **Post 选项**：可以在任何时候设置
   - 带宽控制、同步模式、超时等

#### 构造时设置选项

```cpp
// 在构造时批量设置选项
std::map<std::string, std::string> options = {
    // Pre-bind 选项
    {"mss", "1500"},
    {"udp_rcvbuf", "12582912"},
    
    // Pre 选项
    {"latency", "120"},
    {"messageapi", "1"},
    
    // Post 选项
    {"maxbw", "0"},
    {"inputbw", "0"}
};

// Socket 会自动在正确的时机应用这些选项
SrtSocket socket(options, reactor);
SrtAcceptor acceptor(options, reactor);
```

#### 自动时机管理

系统会自动在正确的时机应用选项：

```cpp
// 内部实现
class SrtSocket {
    bool apply_pre_bind_options();  // 在 bind 之前调用
    bool apply_pre_options();        // 在 connect/listen 之前调用
    bool apply_post_options();       // 在连接建立后调用
};
```

### 3. 增强的错误处理

- 所有 SRT 错误都转换为 `std::error_code`
- 详细的错误日志，包含错误码和描述
- 回调中的异常会被捕获并记录

### 4. 改进的日志系统

支持彩色日志输出：

```cpp
SrtReactor::set_log_callback([](LogLevel level, const char* area, const char* message) {
    const char* color_start = "";
    switch (level) {
        case LogLevel::Debug:    color_start = "\033[36m"; break;  // 青色
        case LogLevel::Notice:   color_start = "\033[32m"; break;  // 绿色
        case LogLevel::Warning:  color_start = "\033[33m"; break;  // 黄色
        case LogLevel::Error:    color_start = "\033[31m"; break;  // 红色
        case LogLevel::Critical: color_start = "\033[35m"; break;  // 紫色
    }
    std::cout << color_start << "[" << level << "] " << message << "\033[0m" << std::endl;
});
```

## 使用示例

### 服务器端

```cpp
// 创建带选项的 acceptor
std::map<std::string, std::string> options = {
    {"latency", "120"},
    {"rcvbuf", "8388608"},
    {"messageapi", "1"}
};

SrtAcceptor acceptor(options);

// 设置访问控制
acceptor.set_listener_callback([](SrtSocket& client, int hsversion, const std::string& streamid) -> int {
    // 基于 stream ID 的访问控制
    if (is_blacklisted(streamid)) {
        return -1;  // 拒绝
    }
    
    // 基于 stream ID 设置配置文件
    apply_profile(client, streamid);
    
    return 0;  // 接受
});

acceptor.bind(9000);
```

### 客户端

```cpp
// 创建带选项的 socket
std::map<std::string, std::string> options = {
    {"streamid", "myapp/user123"},
    {"latency", "50"},
    {"conntimeo", "3000"}
};

SrtSocket socket(options);

// 设置连接状态通知
socket.set_connect_callback([](const std::error_code& ec, SrtSocket& sock) {
    if (!ec) {
        log_connection_established(sock);
    }
});

co_await socket.async_connect("server.example.com", 9000);
```

## 兼容性说明

- 保持了原有 API 的兼容性
- 新功能是可选的，不影响现有代码
- 支持 SRT 1.4.0+ 版本（部分高级选项需要 1.5.0+）

## 性能优化

1. **减少选项设置开销**：批量设置，自动分组应用
2. **原生回调效率更高**：避免了轮询和状态检查
3. **更好的错误处理**：早期拒绝无效连接

## 最佳实践

1. **使用构造时选项设置**：避免手动管理选项时机
2. **利用 Stream ID**：实现灵活的访问控制和配置管理
3. **设置合适的超时**：避免资源泄露
4. **监控统计信息**：及时发现网络问题

## 迁移指南

从旧版本迁移：

```cpp
// 旧代码
SrtAcceptor acceptor;
acceptor.set_option("latency=120");
acceptor.set_option("rcvbuf=8388608");
acceptor.bind(9000);

// 新代码（推荐）
std::map<std::string, std::string> options = {
    {"latency", "120"},
    {"rcvbuf", "8388608"}
};
SrtAcceptor acceptor(options);
acceptor.bind(9000);
```

回调迁移：

```cpp
// 旧的监听回调（应用层）
acceptor.set_listener_callback([](const std::error_code& ec, SrtSocket client) {
    // ...
});

// 新的监听回调（原生）
acceptor.set_listener_callback([](SrtSocket& client, int hsversion, const std::string& streamid) -> int {
    // 返回 0 接受，-1 拒绝
    return 0;
});
```
