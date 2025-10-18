# SRT 回调和选项使用指南

## 概述

本文档详细说明了 SRT Socket 和 Acceptor 中回调机制和选项设置的正确使用方法，以及与 SRT 原生接口的关系。

## SRT 原生回调接口

### 1. Listen Callback（监听回调）

SRT 提供了原生的 C 风格监听回调接口：

```c
typedef int srt_listen_callback_fn(
    void* opaq,                       // 用户数据指针
    SRTSOCKET ns,                     // 新接受的 socket
    int hsversion,                    // 握手版本
    const struct sockaddr* peeraddr,  // 对端地址
    const char* streamid              // Stream ID
);

int srt_listen_callback(SRTSOCKET lsn, srt_listen_callback_fn* hook_fn, void* hook_opaque);
```

**当前实现的封装**：

```cpp
acceptor.set_listener_callback([](const std::error_code& ec, SrtSocket client) {
    if (!ec) {
        // 可以在这里：
        // 1. 检查客户端信息
        // 2. 设置客户端特定选项
        // 3. 决定是否接受连接
        std::cout << "New client: " << client.remote_address() << std::endl;
    }
});
```

### 2. Connect Callback（连接回调）

SRT 提供了原生的连接回调接口：

```c
typedef void srt_connect_callback_fn(
    void* opaq,                       // 用户数据指针
    SRTSOCKET ns,                     // 连接的 socket
    int errorcode,                    // 错误码
    const struct sockaddr* peeraddr,  // 对端地址
    int token                         // 连接 token
);

int srt_connect_callback(SRTSOCKET clr, srt_connect_callback_fn* hook_fn, void* hook_opaque);
```

**当前实现的封装**：

```cpp
socket.set_connect_callback([](const std::error_code& ec, SrtSocket& sock) {
    if (!ec) {
        std::cout << "Connected to " << sock.remote_address() << std::endl;
        
        // 可以在这里获取连接信息
        SRT_TRACEBSTATS stats;
        sock.get_stats(stats);
    }
});
```

## Socket 选项设置时机

根据 SRT 官方文档，选项按设置时机分为三类：

### pre-bind 选项

**必须在 `srt_bind` 调用之前设置**

这些选项影响底层 UDP socket 或绑定行为：

| 选项 | 说明 | 默认值 | 推荐值 |
|------|------|--------|--------|
| `SRTO_MSS` | 最大段大小 | 1500 | 1500 (标准 MTU) |
| `SRTO_RCVBUF` | 接收缓冲区 | 8192 bufs | 根据带宽调整 |
| `SRTO_SNDBUF` | 发送缓冲区 | 8192 bufs | 根据带宽调整 |
| `SRTO_UDP_RCVBUF` | UDP 接收缓冲区 | 8192 bufs | 大文件传输时增大 |
| `SRTO_UDP_SNDBUF` | UDP 发送缓冲区 | 65536 | 大文件传输时增大 |
| `SRTO_IPTOS` | IP 服务类型 | 系统默认 | 0xB8 (实时流) |
| `SRTO_IPTTL` | IP 生存时间 | 64 | 64 |
| `SRTO_REUSEADDR` | 地址复用 | true | true |

**示例**：

```cpp
SrtAcceptor acceptor(reactor);

// 必须在 bind 前设置
acceptor.set_options({
    {"mss", "1500"},
    {"rcvbuf", "16777216"},      // 16MB
    {"sndbuf", "16777216"},      // 16MB
    {"udp_rcvbuf", "16777216"},
    {"udp_sndbuf", "8388608"}
});

acceptor.bind(9000);  // 现在才能 bind
```

### pre 选项

**必须在连接建立之前设置**

这些选项影响连接协商和传输行为：

| 选项 | 说明 | 默认值 | 推荐值 |
|------|------|--------|--------|
| `SRTO_CONNTIMEO` | 连接超时 | 3000 ms | 5000 (慢速网络) |
| `SRTO_LATENCY` | 延迟 | 120 ms | 200 (直播) |
| `SRTO_RCVLATENCY` | 接收延迟 | 120 ms | 同 latency |
| `SRTO_PEERLATENCY` | 对端延迟 | 0 | 同 latency |
| `SRTO_MESSAGEAPI` | 消息模式 | true | true (推荐) |
| `SRTO_PAYLOADSIZE` | 负载大小 | 1316 | 1316 |
| `SRTO_PASSPHRASE` | 加密密码 | "" | 10-79 字符 |
| `SRTO_PBKEYLEN` | 密钥长度 | 0 | 16/24/32 |
| `SRTO_FC` | 流控窗口 | 25600 | 根据延迟调整 |
| `SRTO_TRANSTYPE` | 传输类型 | LIVE | LIVE/FILE |

**示例**：

```cpp
SrtSocket socket(reactor);

// 必须在连接前设置
socket.set_options({
    {"latency", "200"},
    {"rcvlatency", "200"},
    {"peerlatency", "200"},
    {"messageapi", "1"},
    {"payloadsize", "1316"},
    {"fc", "32768"},
    {"conntimeo", "5000"}
});

// 可选：加密设置
socket.set_options({
    {"passphrase", "mySecretPassword123"},
    {"pbkeylen", "32"}  // AES-256
});

co_await socket.async_connect("192.168.1.100", 9000);
```

### post 选项

**可以在任何时候设置，包括连接后**

这些选项主要控制运行时行为：

| 选项 | 说明 | 默认值 | 用途 |
|------|------|--------|------|
| `SRTO_MAXBW` | 最大带宽 | -1 (无限) | 限制发送速率 |
| `SRTO_INPUTBW` | 输入带宽 | 0 (自动) | 配合 OHEADBW 使用 |
| `SRTO_OHEADBW` | 开销百分比 | 25% | 重传开销 |
| `SRTO_RCVSYN` | 接收阻塞 | true | 控制阻塞行为 |
| `SRTO_SNDSYN` | 发送阻塞 | true | 控制阻塞行为 |
| `SRTO_RCVTIMEO` | 接收超时 | -1 (无限) | 超时控制 |
| `SRTO_SNDTIMEO` | 发送超时 | -1 (无限) | 超时控制 |
| `SRTO_LINGER` | 关闭延迟 | 0 (Live) | 优雅关闭 |

**示例**：

```cpp
SrtSocket socket(reactor);
co_await socket.async_connect("192.168.1.100", 9000);

// 连接后可以设置这些选项
socket.set_options({
    {"maxbw", "10000000"},    // 限制为 10 Mbps
    {"rcvtimeo", "5000"},     // 5秒超时
    {"sndtimeo", "5000"}
});
```

## 实际应用场景

### 场景 1：直播服务器（高吞吐量）

```cpp
SrtAcceptor acceptor(reactor);

// pre-bind: 大缓冲区
acceptor.set_options({
    {"rcvbuf", "33554432"},       // 32MB
    {"sndbuf", "33554432"},       // 32MB
    {"udp_rcvbuf", "33554432"},
    {"udp_sndbuf", "16777216"}
});

// pre: 低延迟配置
acceptor.set_options({
    {"latency", "120"},           // 120ms 低延迟
    {"rcvlatency", "120"},
    {"messageapi", "1"},
    {"payloadsize", "1456"},      // 更大的负载
    {"fc", "65536"}               // 更大的流控窗口
});

acceptor.bind(9000);

// 连接后：限制客户端带宽
acceptor.set_listener_callback([](const std::error_code& ec, SrtSocket client) {
    if (!ec) {
        client.set_option("maxbw=20000000");  // 每客户端 20 Mbps
    }
});
```

### 场景 2：文件传输（高可靠性）

```cpp
SrtSocket socket(reactor);

// pre-bind: 大缓冲区
socket.set_options({
    {"rcvbuf", "67108864"},       // 64MB
    {"sndbuf", "67108864"},       // 64MB
});

// pre: 文件模式，高延迟
socket.set_options({
    {"transtype", "1"},           // FILE mode (SRTT_FILE)
    {"latency", "500"},           // 500ms 高延迟以容忍更多重传
    {"messageapi", "0"},          // Stream API for file
    {"fc", "65536"}
});

co_await socket.async_connect("192.168.1.100", 9000);

// post: 无带宽限制
socket.set_option("maxbw=-1");
```

### 场景 3：加密传输

```cpp
SrtSocket socket(reactor);

// pre: 加密配置必须在连接前设置
socket.set_options({
    {"passphrase", "myVerySecurePassword123"},
    {"pbkeylen", "32"},           // AES-256
    {"latency", "200"},
    {"messageapi", "1"}
});

// 设置连接回调检查加密状态
socket.set_connect_callback([](const std::error_code& ec, SrtSocket& sock) {
    if (!ec) {
        // 可以检查加密状态（需要通过 srt_getsockopt 获取 SRTO_KMSTATE）
        std::cout << "Secure connection established" << std::endl;
    }
});

co_await socket.async_connect("192.168.1.100", 9000);
```

### 场景 4：多客户端差异化服务

```cpp
SrtAcceptor acceptor(reactor);

// 基础配置
acceptor.set_options({
    {"latency", "200"},
    {"messageapi", "1"}
});

acceptor.bind(9000);

// 根据客户端 IP 设置不同策略
acceptor.set_listener_callback([](const std::error_code& ec, SrtSocket client) {
    if (ec) return;
    
    std::string peer_addr = client.remote_address();
    
    if (peer_addr.find("192.168.") == 0) {
        // 内网客户端：无限制
        std::cout << "Internal client: " << peer_addr << std::endl;
        client.set_option("maxbw=-1");
        
    } else if (peer_addr.find("10.") == 0) {
        // VIP 客户端：高带宽
        std::cout << "VIP client: " << peer_addr << std::endl;
        client.set_option("maxbw=20000000");  // 20 Mbps
        
    } else {
        // 普通外网客户端：限制带宽
        std::cout << "Regular client: " << peer_addr << std::endl;
        client.set_option("maxbw=5000000");   // 5 Mbps
    }
});
```

## 选项设置的最佳实践

### 1. 分阶段设置

```cpp
// 阶段 1: 创建 socket
SrtSocket socket(reactor);

// 阶段 2: pre-bind 选项
socket.set_options({
    {"mss", "1500"},
    {"rcvbuf", "16777216"}
});

// 阶段 3: pre 选项
socket.set_options({
    {"latency", "200"},
    {"messageapi", "1"}
});

// 阶段 4: 连接
co_await socket.async_connect("192.168.1.100", 9000);

// 阶段 5: post 选项（可选）
socket.set_option("maxbw=10000000");
```

### 2. 使用配置文件

```cpp
struct ServerConfig {
    // pre-bind options
    int mss = 1500;
    int rcvbuf = 16777216;
    int sndbuf = 16777216;
    
    // pre options
    int latency = 200;
    bool messageapi = true;
    int payloadsize = 1316;
    
    // post options
    int64_t maxbw = -1;
    
    std::map<std::string, std::string> to_pre_bind_options() const {
        return {
            {"mss", std::to_string(mss)},
            {"rcvbuf", std::to_string(rcvbuf)},
            {"sndbuf", std::to_string(sndbuf)}
        };
    }
    
    std::map<std::string, std::string> to_pre_options() const {
        return {
            {"latency", std::to_string(latency)},
            {"messageapi", messageapi ? "1" : "0"},
            {"payloadsize", std::to_string(payloadsize)}
        };
    }
};

// 使用
ServerConfig config;
acceptor.set_options(config.to_pre_bind_options());
acceptor.set_options(config.to_pre_options());
acceptor.bind(port);
```

### 3. 错误处理

```cpp
// 检查选项设置是否成功
if (!socket.set_options(options)) {
    ASRT_LOG_WARNING("Some options failed to set");
    
    // 可以尝试单独设置并检查每个选项
    for (const auto& [key, value] : options) {
        if (!socket.set_option(key + "=" + value)) {
            ASRT_LOG_ERROR("Failed to set option: " + key);
        }
    }
}
```

## 常见问题

### Q1: 为什么某些选项设置失败？

**A**: 检查以下几点：
1. 选项设置时机是否正确（pre-bind/pre/post）
2. 选项值是否在有效范围内
3. 某些选项之间有依赖关系（如 RCVBUF 不能大于 FC）

### Q2: 如何知道选项是否生效？

**A**: 可以在连接建立后使用 `srt_getsockopt` 读取选项值（需要扩展当前实现）。

### Q3: Callback 在哪个线程中执行？

**A**: 
- `connect_callback`: 在 SRT reactor 的 ASIO 线程中执行
- `listener_callback`: 在 accept 协程的线程中执行

需要注意线程安全。

### Q4: 能否在 callback 中拒绝连接？

**A**: SRT 的 `srt_listen_callback` 返回 -1 可以拒绝连接。当前实现需要扩展以支持此功能。

## 参考资源

- [SRT API Socket Options 文档](depends/build/srt/srt-1.5.4/docs/API/API-socket-options.md)
- [SRT Native Callbacks 说明](docs/SRT_NATIVE_CALLBACKS.md)
- [高级服务器示例](examples/srt_advanced_server.cpp)

## 总结

正确使用 SRT 的选项和回调需要：

1. **理解选项时机**：pre-bind、pre、post 三种类型
2. **合理配置参数**：根据应用场景选择合适的值
3. **善用回调机制**：实现动态配置和访问控制
4. **完善错误处理**：检查选项设置结果和连接状态

遵循这些原则，可以构建高性能、高可靠的 SRT 应用。

