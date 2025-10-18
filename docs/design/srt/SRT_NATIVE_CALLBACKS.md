# SRT 原生回调接口使用说明

## 概述

SRT 提供了两个原生的 C 风格回调接口，当前的实现提供了 C++ 封装，但了解底层接口有助于更好地理解其工作原理。

## SRT 原生回调接口

### 1. Listen Callback（监听回调）

```c
typedef int srt_listen_callback_fn(
    void* opaq,              // 用户自定义数据指针
    SRTSOCKET ns,            // 新接受的 socket
    int hsversion,           // 握手版本 (4 或 5)
    const struct sockaddr* peeraddr,  // 对端地址
    const char* streamid     // Stream ID (可能为 NULL)
);

int srt_listen_callback(
    SRTSOCKET lsn,                    // 监听 socket
    srt_listen_callback_fn* hook_fn,  // 回调函数指针
    void* hook_opaque                 // 传递给回调的用户数据
);
```

**回调返回值**：
- `0`: 接受连接
- `-1`: 拒绝连接

**触发时机**：
- 在 `srt_accept` 之前调用
- 可以在回调中设置新 socket 的选项
- 可以根据 streamid 或 peeraddr 决定是否接受连接

### 2. Connect Callback（连接回调）

```c
typedef void srt_connect_callback_fn(
    void* opaq,              // 用户自定义数据指针
    SRTSOCKET ns,            // 连接的 socket
    int errorcode,           // 错误码 (0 表示成功)
    const struct sockaddr* peeraddr,  // 对端地址
    int token                // 连接 token
);

int srt_connect_callback(
    SRTSOCKET clr,                      // 客户端 socket
    srt_connect_callback_fn* hook_fn,   // 回调函数指针
    void* hook_opaque                   // 传递给回调的用户数据
);
```

**触发时机**：
- 连接建立成功或失败时调用
- 在异步连接模式下特别有用

## 当前实现的封装

当前的 `SrtSocket` 和 `SrtAcceptor` 类提供了 C++ 风格的回调封装：

### SrtSocket 的 Connect Callback

```cpp
socket.set_connect_callback([](const std::error_code& ec, SrtSocket& sock) {
    if (ec) {
        std::cerr << "Connection failed: " << ec.message() << std::endl;
    } else {
        std::cout << "Connected to " << sock.remote_address() << std::endl;
    }
});
```

**实现说明**：
- 内部使用 SRT 原生的 `srt_connect_callback`
- 将错误码转换为 `std::error_code`
- 提供 socket 引用以便获取连接信息

### SrtAcceptor 的 Listener Callback

```cpp
acceptor.set_listener_callback([](const std::error_code& ec, SrtSocket client) {
    if (ec) {
        std::cerr << "Accept error: " << ec.message() << std::endl;
    } else {
        std::cout << "New client from " << client.remote_address() << std::endl;
    }
});
```

**实现说明**：
- 内部使用 SRT 原生的 `srt_listen_callback`
- 自动创建 `SrtSocket` 对象封装新连接
- 可以在回调中检查并拒绝连接（返回值控制）

## Socket 选项设置时机

根据 SRT 文档，选项分为三种类型：

### 1. pre-bind 选项
**必须在 `srt_bind` 之前设置**

常见选项：
- `SRTO_BINDTODEVICE` - 绑定到特定网络设备
- `SRTO_MSS` - 最大段大小
- `SRTO_RCVBUF` / `SRTO_SNDBUF` - 缓冲区大小
- `SRTO_UDP_RCVBUF` / `SRTO_UDP_SNDBUF` - UDP 缓冲区大小
- `SRTO_IPTOS` / `SRTO_IPTTL` - IP 选项
- `SRTO_IPV6ONLY` - IPv6 专用模式
- `SRTO_REUSEADDR` - 地址复用

**示例**：
```cpp
SrtSocket socket(reactor);

// 这些必须在连接前设置
socket.set_options({
    {"mss", "1500"},
    {"rcvbuf", "8388608"},
    {"sndbuf", "8388608"}
});

co_await socket.async_connect("192.168.1.100", 9000);
```

### 2. pre 选项
**必须在连接建立前设置**

常见选项：
- `SRTO_CONNTIMEO` - 连接超时
- `SRTO_LATENCY` - 延迟
- `SRTO_RCVLATENCY` / `SRTO_PEERLATENCY` - 接收/对端延迟
- `SRTO_MESSAGEAPI` - 消息 API 模式
- `SRTO_PASSPHRASE` - 加密密码
- `SRTO_PBKEYLEN` - 加密密钥长度
- `SRTO_TRANSTYPE` - 传输类型 (Live/File)
- `SRTO_CONGESTION` - 拥塞控制算法
- `SRTO_PAYLOADSIZE` - 负载大小

**示例**：
```cpp
SrtSocket socket(reactor);

// 这些必须在连接前设置
socket.set_options({
    {"latency", "200"},
    {"messageapi", "1"},
    {"passphrase", "mypassword"},
    {"pbkeylen", "32"}
});

co_await socket.async_connect("192.168.1.100", 9000);
```

### 3. post 选项
**可以在任何时候设置**

常见选项：
- `SRTO_RCVSYN` / `SRTO_SNDSYN` - 阻塞/非阻塞模式
- `SRTO_RCVTIMEO` / `SRTO_SNDTIMEO` - 读写超时
- `SRTO_LINGER` - 关闭时的延迟
- `SRTO_MAXBW` - 最大带宽
- `SRTO_INPUTBW` - 输入带宽
- `SRTO_OHEADBW` - 开销带宽百分比
- `SRTO_DRIFTTRACER` - 时间漂移跟踪

**示例**：
```cpp
SrtSocket socket(reactor);
co_await socket.async_connect("192.168.1.100", 9000);

// 连接后也可以设置这些选项
socket.set_options({
    {"maxbw", "10000000"},  // 10 Mbps
    {"rcvtimeo", "5000"}    // 5秒超时
});
```

## 当前实现的选项处理

当前实现在连接/绑定前应用所有选项，这对大多数情况是安全的：

```cpp
// SrtSocket::async_connect 中
bool SrtSocket::apply_options() {
    return options_.apply_to_socket(sock_);
}

// 在连接前调用
if (!apply_options()) {
    ASRT_LOG_WARNING("Some socket options failed to apply");
}
```

## 使用建议

### 1. 在 Listener Callback 中设置选项

```cpp
acceptor.set_listener_callback([](const std::error_code& ec, SrtSocket client) {
    if (!ec) {
        // 可以根据 streamid 或其他信息为不同客户端设置不同选项
        client.set_options({
            {"maxbw", "5000000"}  // 限制此客户端带宽
        });
    }
});
```

### 2. 使用 Connect Callback 检查连接状态

```cpp
socket.set_connect_callback([](const std::error_code& ec, SrtSocket& sock) {
    if (!ec) {
        // 连接成功，获取统计信息
        SRT_TRACEBSTATS stats;
        if (sock.get_stats(stats)) {
            std::cout << "RTT: " << stats.msRTT << " ms" << std::endl;
        }
    }
});
```

### 3. 正确的选项设置顺序

```cpp
SrtSocket socket(reactor);

// 1. 先设置 pre-bind 选项
socket.set_option("mss=1500");
socket.set_option("rcvbuf=8388608");

// 2. 再设置 pre 选项
socket.set_option("latency=200");
socket.set_option("messageapi=1");

// 3. 连接
co_await socket.async_connect("192.168.1.100", 9000);

// 4. 连接后可以设置 post 选项
socket.set_option("maxbw=10000000");
```

## 高级用法：Stream ID

Stream ID 是 SRT 1.3.0+ 引入的功能，用于访问控制和路由：

```cpp
// 客户端设置 Stream ID
socket.set_option("streamid=#!::u=user,r=resource,m=publish");
co_await socket.async_connect("192.168.1.100", 9000);

// 服务器端在 listener callback 中获取 Stream ID
// 注意：当前实现需要扩展以支持获取 Stream ID
```

## 注意事项

1. **选项设置时机**：违反时机限制会导致选项设置失败，但不会抛出异常，只会记录警告日志

2. **回调中的异常**：回调函数中抛出的异常会被捕获并记录，不会传播

3. **线程安全**：回调在 SRT 内部线程中执行，需要注意线程安全

4. **选项验证**：并非所有选项组合都有效，某些选项之间有依赖关系

## 扩展建议

如果需要更细粒度的控制，可以考虑以下扩展：

1. **暴露原生回调**：允许用户直接注册 SRT 原生回调

2. **Stream ID 支持**：添加获取和解析 Stream ID 的接口

3. **选项验证**：在设置前验证选项的时机和值范围

4. **选项分组**：将选项按 pre-bind/pre/post 分组管理

## 参考资源

- [SRT API 文档](https://github.com/Haivision/srt/blob/master/docs/API/API.md)
- [SRT Socket 选项](https://github.com/Haivision/srt/blob/master/docs/API/API-socket-options.md)
- [SRT 访问控制](https://github.com/Haivision/srt/blob/master/docs/features/access-control.md)

