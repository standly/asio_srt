# 错误处理指南

## 概述

`asio_srt` 使用标准的 C++ `std::error_code` 体系，与 ASIO 和其他标准库组件完全兼容。

## 错误码类型

### 1. 标准 ASIO 错误码

我们优先使用标准的 `asio::error` 和 `std::errc` 错误码：

| 错误码 | 使用场景 | 说明 |
|--------|---------|------|
| `asio::error::operation_aborted` | 操作被取消 | 协程取消、超时取消等 |
| `std::errc::timed_out` | 操作超时 | 带超时的等待操作 |
| `std::errc::operation_would_block` | 非阻塞操作 | SRT 异步操作需要等待 |
| `std::errc::bad_file_descriptor` | 无效的 socket | Socket 无效或已关闭 |
| `std::errc::connection_refused` | 连接被拒绝 | SRT 连接失败 |
| `std::errc::connection_reset` | 连接断开 | SRT 连接丢失 |

### 2. SRT 特定错误码

对于 SRT 特有的错误，我们定义了 `asrt::srt_errc` 枚举：

```cpp
namespace asrt {
    enum class srt_errc {
        success = 0,
        
        // 连接相关错误
        connection_setup = 1000,      // 连接建立失败
        connection_rejected = 1001,   // 连接被拒绝
        connection_lost = 1002,       // 连接丢失
        
        // 资源相关错误
        resource_fail = 2000,         // 资源分配失败
        thread_fail = 2001,           // 线程创建失败
        
        // 操作相关错误
        invalid_socket = 3000,        // 无效的 socket
        epoll_add_failed = 3001,      // 添加到 epoll 失败
        epoll_update_failed = 3002,   // 更新 epoll 失败
        
        // 数据传输错误
        send_failed = 4000,           // 发送失败
        recv_failed = 4001,           // 接收失败
        
        // 超时
        timeout = 5000,               // 操作超时
    };
}
```

## 错误处理示例

### 1. 使用异常处理（推荐）

```cpp
#include "asrt/srt_reactor.h"
#include <iostream>

asio::awaitable<void> read_with_timeout() {
    SRTSOCKET sock = /* ... */;
    
    try {
        // 带超时的等待
        int events = co_await reactor.async_wait_readable(sock, 5000ms);
        
        // 读取数据
        char buffer[1500];
        int n = srt_recv(sock, buffer, sizeof(buffer));
        
        if (n > 0) {
            std::cout << "Received " << n << " bytes\n";
        }
        
    } catch (const asio::system_error& e) {
        if (e.code() == std::errc::timed_out) {
            std::cerr << "Timeout waiting for data\n";
        } else if (e.code() == std::errc::connection_reset) {
            std::cerr << "Connection lost\n";
        } else {
            std::cerr << "Error: " << e.what() << "\n";
        }
    }
}
```

### 2. 错误码映射

SRT 原生错误会自动映射到标准错误码：

```cpp
#include "asrt/srt_error.h"

void handle_srt_error() {
    // 获取 SRT 最后的错误
    std::error_code ec = asrt::make_srt_error_code();
    
    // 标准化处理
    if (ec == std::errc::connection_refused) {
        std::cerr << "Connection refused\n";
    } else if (ec == std::errc::operation_would_block) {
        std::cerr << "Would block (non-blocking mode)\n";
    } else if (ec.category() == asrt::srt_category()) {
        std::cerr << "SRT specific error: " << ec.message() << "\n";
    }
}
```

### 3. 比较错误码

```cpp
// ✅ 正确 - 使用 make_error_code
if (e.code() == std::make_error_code(std::errc::timed_out)) {
    // 处理超时
}

// ✅ 正确 - 直接比较标准错误码
if (e.code() == asio::error::operation_aborted) {
    // 处理取消
}

// ❌ 错误 - 不能直接比较枚举
if (e.code() == std::errc::timed_out) {  // 编译错误！
    // ...
}
```

### 4. 获取详细错误信息

```cpp
#include "asrt/srt_error.h"

void detailed_error_info() {
    const char* error_msg = nullptr;
    std::error_code ec = asrt::make_srt_error_code(error_msg);
    
    std::cerr << "Error code: " << ec.value() << "\n";
    std::cerr << "Category: " << ec.category().name() << "\n";
    std::cerr << "Message: " << ec.message() << "\n";
    std::cerr << "SRT details: " << error_msg << "\n";
}
```

## 错误码转换表

### SRT 原生错误 → 标准错误码

| SRT 错误码 | 映射到的标准错误 | 说明 |
|-----------|----------------|------|
| `SRT_EINVSOCK` | `std::errc::bad_file_descriptor` | 无效的 socket |
| `SRT_ECONNSETUP` | `asrt::srt_errc::connection_setup` | 连接建立失败 |
| `SRT_ECONNREJ` | `std::errc::connection_refused` | 连接被拒绝 |
| `SRT_ECONNLOST` | `std::errc::connection_reset` | 连接断开 |
| `SRT_ERESOURCE` | `std::errc::not_enough_memory` | 资源不足 |
| `SRT_ETHREAD` | `std::errc::resource_unavailable_try_again` | 线程创建失败 |
| `SRT_EASYNCSND`/`SRT_EASYNCRCV` | `std::errc::operation_would_block` | 需要等待 |
| `SRT_ETIMEOUT` | `asrt::srt_errc::timeout` | 超时 |

## 与 ASIO 其他协议的兼容性

### TCP/UDP 示例对比

```cpp
// TCP 使用 ASIO
asio::awaitable<void> tcp_example(asio::ip::tcp::socket& sock) {
    try {
        char buffer[1024];
        size_t n = co_await sock.async_read_some(
            asio::buffer(buffer), 
            asio::use_awaitable
        );
    } catch (const asio::system_error& e) {
        if (e.code() == asio::error::eof) {
            // 连接关闭
        } else if (e.code() == asio::error::operation_aborted) {
            // 操作取消
        }
    }
}

// SRT 使用 asio_srt - 完全相同的错误处理模式！
asio::awaitable<void> srt_example(SRTSOCKET sock) {
    try {
        co_await reactor.async_wait_readable(sock);
        
        char buffer[1024];
        int n = srt_recv(sock, buffer, sizeof(buffer));
        
    } catch (const asio::system_error& e) {
        if (e.code() == std::errc::connection_reset) {
            // 连接关闭
        } else if (e.code() == asio::error::operation_aborted) {
            // 操作取消
        }
    }
}
```

## 超时处理的变化

### 旧版 API（返回 -1 表示超时）❌

```cpp
// 旧版本 - 不推荐
int result = co_await reactor.async_wait_readable(sock, 1000ms);
if (result == -1) {
    // 超时
} else if (result >= 0) {
    // 成功
}
```

### 新版 API（抛出异常）✅

```cpp
// 新版本 - 推荐
try {
    int events = co_await reactor.async_wait_readable(sock, 1000ms);
    // 成功，events 包含事件标志
    
} catch (const asio::system_error& e) {
    if (e.code() == std::make_error_code(std::errc::timed_out)) {
        // 超时
    } else {
        // 其他错误
    }
}
```

**为什么改变？**
1. ✅ **一致性**：与 ASIO 标准错误处理一致
2. ✅ **类型安全**：不会混淆返回值和错误码
3. ✅ **可组合性**：更容易与其他 ASIO 组件集成
4. ✅ **强制处理**：异常不会被忽略

## 最佳实践

### 1. ✅ 使用标准错误码

优先使用 `std::errc` 和 `asio::error`，只在必要时使用 `asrt::srt_errc`。

### 2. ✅ 统一的错误处理

```cpp
asio::awaitable<void> unified_error_handling() {
    try {
        // 各种操作
        co_await reactor.async_wait_readable(sock, 5s);
        co_await reactor.async_wait_writable(sock);
        
    } catch (const asio::system_error& e) {
        // 统一处理所有错误
        std::cerr << "Error: " << e.what() << "\n";
        
        // 根据需要特殊处理某些错误
        if (e.code() == std::make_error_code(std::errc::timed_out)) {
            // 重试逻辑
        }
    }
}
```

### 3. ✅ 错误分类处理

```cpp
void handle_error(const std::error_code& ec) {
    if (ec.category() == std::generic_category()) {
        // 标准 C++ 错误
        std::cerr << "Standard error: " << ec.message() << "\n";
        
    } else if (ec.category() == asio::error::get_system_category()) {
        // ASIO 系统错误
        std::cerr << "ASIO error: " << ec.message() << "\n";
        
    } else if (ec.category() == asrt::srt_category()) {
        // SRT 特定错误
        std::cerr << "SRT error: " << ec.message() << "\n";
    }
}
```

### 4. ✅ 错误日志

```cpp
void log_error(const std::error_code& ec, const std::string& context) {
    std::cerr << "[ERROR] " << context << ": "
              << ec.message() 
              << " (code=" << ec.value() 
              << ", category=" << ec.category().name() 
              << ")\n";
}

// 使用
try {
    co_await reactor.async_wait_readable(sock, 1s);
} catch (const asio::system_error& e) {
    log_error(e.code(), "waiting for data");
}
```

## 总结

### 关键要点

1. **使用标准错误码** - 与 ASIO 和 C++ 标准库兼容
2. **异常优先** - 使用异常处理而非返回码
3. **正确比较** - 使用 `std::make_error_code()` 创建错误码
4. **统一处理** - `asio::system_error` 捕获所有错误
5. **分类映射** - SRT 错误自动映射到标准错误

### 兼容性保证

- ✅ 与 `asio::ip::tcp` 相同的错误处理模式
- ✅ 与 `asio::ip::udp` 相同的错误处理模式  
- ✅ 可与其他 ASIO 协议混合使用
- ✅ 支持 C++20 协程错误传播

这种标准化的错误处理使得 `asio_srt` 可以无缝集成到现有的 ASIO 应用中！

