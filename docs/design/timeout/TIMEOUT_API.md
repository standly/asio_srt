# 超时 API 使用指南

## 概述

SrtReactor 提供了带超时功能的异步等待 API，允许你在指定时间内等待 socket 变为可读或可写状态。如果在超时时间内操作未完成，将返回 `-1` 表示超时。

## API 接口

### 带超时的可读等待

```cpp
asio::awaitable<int> async_wait_readable(
    SRTSOCKET srt_sock, 
    std::chrono::milliseconds timeout
);
```

### 带超时的可写等待

```cpp
asio::awaitable<int> async_wait_writable(
    SRTSOCKET srt_sock, 
    std::chrono::milliseconds timeout
);
```

### 返回值

- **成功**: 返回事件标志（`SRT_EPOLL_IN` 或 `SRT_EPOLL_OUT`，值 > 0）
- **超时**: 返回 `-1`
- **错误**: 抛出 `asio::system_error` 异常

## 使用示例

### 示例 1: 带超时的接收数据

```cpp
#include "asrt/srt_reactor.h"
#include <chrono>

using namespace std::chrono_literals;

asio::awaitable<void> receive_with_timeout(SRTSOCKET sock) {
    auto& reactor = SrtReactor::get_instance();
    
    try {
        // 等待最多 5 秒 socket 变为可读
        int result = co_await reactor.async_wait_readable(sock, 5000ms);
        
        if (result == -1) {
            std::cout << "Timeout: No data received within 5 seconds\n";
            co_return;
        }
        
        // Socket is readable, receive data
        char buffer[1500];
        int received = srt_recv(sock, buffer, sizeof(buffer));
        
        if (received > 0) {
            std::cout << "Received " << received << " bytes\n";
        }
        
    } catch (const asio::system_error& e) {
        std::cerr << "Error: " << e.what() << "\n";
    }
}
```

### 示例 2: 带超时的发送数据

```cpp
asio::awaitable<bool> send_with_timeout(SRTSOCKET sock, const char* data, size_t len) {
    auto& reactor = SrtReactor::get_instance();
    
    try {
        // 等待最多 3 秒 socket 变为可写
        int result = co_await reactor.async_wait_writable(sock, 3000ms);
        
        if (result == -1) {
            std::cout << "Timeout: Socket not writable within 3 seconds\n";
            co_return false;
        }
        
        // Socket is writable, send data
        int sent = srt_send(sock, data, len);
        
        if (sent > 0) {
            std::cout << "Sent " << sent << " bytes\n";
            co_return true;
        }
        
        co_return false;
        
    } catch (const asio::system_error& e) {
        std::cerr << "Error: " << e.what() << "\n";
        co_return false;
    }
}
```

### 示例 3: 重试机制

```cpp
asio::awaitable<bool> receive_with_retry(SRTSOCKET sock, int max_retries = 3) {
    auto& reactor = SrtReactor::get_instance();
    char buffer[1500];
    
    for (int retry = 0; retry < max_retries; ++retry) {
        try {
            // 每次尝试等待 2 秒
            int result = co_await reactor.async_wait_readable(sock, 2000ms);
            
            if (result == -1) {
                std::cout << "Timeout on attempt " << (retry + 1) << "\n";
                continue; // 重试
            }
            
            // 接收数据
            int received = srt_recv(sock, buffer, sizeof(buffer));
            if (received > 0) {
                std::cout << "Successfully received data on attempt " 
                         << (retry + 1) << "\n";
                co_return true;
            }
            
        } catch (const asio::system_error& e) {
            std::cerr << "Error on attempt " << (retry + 1) 
                     << ": " << e.what() << "\n";
        }
    }
    
    std::cout << "Failed after " << max_retries << " attempts\n";
    co_return false;
}
```

### 示例 4: 条件超时

```cpp
asio::awaitable<void> smart_receive(SRTSOCKET sock, bool is_critical) {
    auto& reactor = SrtReactor::get_instance();
    
    // 关键数据使用更长的超时
    auto timeout = is_critical ? 10000ms : 1000ms;
    
    int result = co_await reactor.async_wait_readable(sock, timeout);
    
    if (result == -1) {
        if (is_critical) {
            std::cerr << "Critical timeout - this is serious!\n";
            // 可能需要重新连接或报警
        } else {
            std::cout << "Non-critical timeout - can be ignored\n";
        }
        co_return;
    }
    
    // 处理数据...
}
```

### 示例 5: 并发操作与超时

```cpp
asio::awaitable<void> handle_connection(SRTSOCKET client, SRTSOCKET server) {
    auto& reactor = SrtReactor::get_instance();
    
    // 同时等待两个 socket，使用不同的超时
    asio::co_spawn(
        reactor.get_io_context(),
        [&]() -> asio::awaitable<void> {
            int result = co_await reactor.async_wait_readable(client, 5000ms);
            if (result != -1) {
                std::cout << "Client has data\n";
            }
        },
        asio::detached
    );
    
    asio::co_spawn(
        reactor.get_io_context(),
        [&]() -> asio::awaitable<void> {
            int result = co_await reactor.async_wait_readable(server, 3000ms);
            if (result != -1) {
                std::cout << "Server has data\n";
            }
        },
        asio::detached
    );
}
```

## 实现原理

### 超时机制

1. **创建定时器**: 使用 `asio::steady_timer` 设置超时时间
2. **启动定时器协程**: 独立协程等待定时器到期
3. **超时触发**: 定时器到期时通过 `asio::cancellation_signal` 取消 socket 操作
4. **区分超时**: 通过 `timed_out` 标志区分是超时还是其他取消

### 工作流程图

```
开始
  |
  ├─> 创建 steady_timer（超时时间）
  ├─> 创建 cancellation_signal
  ├─> 启动定时器协程
  |      |
  |      └─> 等待定时器 ──> 到期 ──> 设置 timed_out = true
  |                                └─> 发送 cancellation 信号
  |
  └─> 等待 socket 操作（带 cancellation 支持）
         |
         ├─> Socket 就绪 ──> 取消定时器 ──> 返回事件标志
         |
         └─> 被取消 ──> 检查 timed_out
                          |
                          ├─> true ──> 返回 -1 (超时)
                          |
                          └─> false ──> 抛出异常 (其他错误)
```

## 性能考虑

### 定时器开销

- 每个超时操作会创建一个 `steady_timer` 和一个定时器协程
- 对于高频操作，考虑使用对象池或预分配定时器
- 短超时（< 10ms）可能不够精确，依赖于系统调度器

### 最佳实践

1. **合理设置超时时间**
   - 网络操作：1-10 秒
   - 本地操作：100-500 毫秒
   - 关键操作：可以更长

2. **避免过短的超时**
   ```cpp
   // ❌ 不推荐 - 太短，可能经常超时
   co_await reactor.async_wait_readable(sock, 10ms);
   
   // ✅ 推荐 - 足够的时间
   co_await reactor.async_wait_readable(sock, 1000ms);
   ```

3. **处理超时情况**
   ```cpp
   int result = co_await reactor.async_wait_readable(sock, timeout);
   
   if (result == -1) {
       // 明确处理超时
       log_timeout();
       update_statistics();
       // 决定是重试还是放弃
   }
   ```

4. **组合使用**
   ```cpp
   // 先用无超时版本测试连接
   co_await reactor.async_wait_writable(sock);
   
   // 然后用超时版本处理数据
   while (running) {
       int result = co_await reactor.async_wait_readable(sock, 5000ms);
       if (result == -1) {
           // 超时 - 发送心跳或检查连接
           send_heartbeat();
       }
   }
   ```

## 测试覆盖

项目包含以下超时测试用例：

1. **TimeoutOnReadable** - 验证超时正确发生
2. **ReadableBeforeTimeout** - 验证操作在超时前完成
3. **WritableWithTimeout** - 验证立即可写的情况

运行测试：
```bash
cd build
./tests/test_srt_reactor --gtest_filter=*Timeout*
```

## 错误处理

### 常见错误场景

1. **Socket 已关闭**
   ```cpp
   try {
       co_await reactor.async_wait_readable(closed_sock, 1000ms);
   } catch (const asio::system_error& e) {
       // 处理 socket 错误
   }
   ```

2. **取消操作**
   ```cpp
   // 使用 cancellation_signal 提前取消
   asio::cancellation_signal cancel;
   auto result = co_await asio::async_initiate<...>(...,
       asio::bind_cancellation_slot(cancel.slot(), asio::use_awaitable)
   );
   ```

3. **超时与错误的区分**
   ```cpp
   try {
       int result = co_await reactor.async_wait_readable(sock, 1000ms);
       
       if (result == -1) {
           // 纯超时 - 不是错误
           handle_timeout();
       } else {
           // 成功
           process_data();
       }
       
   } catch (const asio::system_error& e) {
       // 真正的错误（如 socket 关闭）
       handle_error(e);
   }
   ```

## 总结

带超时的 API 提供了：
- ✅ 灵活的超时控制
- ✅ 清晰的超时指示（返回 -1）
- ✅ 与现有 API 兼容
- ✅ 完整的错误处理
- ✅ 高性能实现

适用于需要可靠超时控制的场景，如网络通信、请求/响应模式、心跳检测等。


