# 超时功能使用指南

## 概述

`async_queue` 现在支持带超时的读取操作，让你可以在指定时间内等待消息，如果超时则返回错误。

## 新增 API

### 1. 单消息超时读取

```cpp
template<typename Duration>
auto async_read_msg_with_timeout(Duration timeout, CompletionToken token);
```

**功能：**
- 在指定时间内等待并读取一条消息
- 如果超时，返回 `asio::error::timed_out`
- 如果队列中已有消息，立即返回（不等待）

**使用示例：**
```cpp
using namespace std::chrono_literals;

// 等待最多 5 秒
auto [ec, msg] = co_await queue->async_read_msg_with_timeout(5s, use_awaitable);

if (ec == asio::error::timed_out) {
    std::cout << "超时！" << std::endl;
} else if (ec) {
    std::cout << "错误: " << ec.message() << std::endl;
} else {
    std::cout << "收到消息: " << msg << std::endl;
}
```

### 2. 批量消息超时读取

```cpp
template<typename Duration>
auto async_read_msgs_with_timeout(size_t max_count, Duration timeout, CompletionToken token);
```

**功能：**
- 在指定时间内等待并读取最多 `max_count` 条消息
- 如果超时且队列为空，返回 `asio::error::timed_out` 和空的 vector
- 如果超时但队列中有部分消息，返回这些消息（无错误）
- 如果队列中已有足够消息，立即返回

**使用示例：**
```cpp
// 等待最多 3 秒，读取最多 10 条消息
auto [ec, messages] = co_await queue->async_read_msgs_with_timeout(10, 3s, use_awaitable);

if (ec == asio::error::timed_out) {
    std::cout << "超时且无消息" << std::endl;
} else if (ec) {
    std::cout << "错误: " << ec.message() << std::endl;
} else {
    std::cout << "收到 " << messages.size() << " 条消息" << std::endl;
}
```

## 使用场景

### 场景 1: 防止永久阻塞

```cpp
awaitable<void> reader(auto queue) {
    while (true) {
        // 最多等待 10 秒
        auto [ec, msg] = co_await queue->async_read_msg_with_timeout(10s, use_awaitable);
        
        if (ec == asio::error::timed_out) {
            // 检查是否应该退出
            if (should_stop) {
                break;
            }
            continue;
        }
        
        if (ec) {
            break;
        }
        
        process(msg);
    }
}
```

### 场景 2: 定期任务

```cpp
awaitable<void> worker(auto queue) {
    while (true) {
        auto [ec, msg] = co_await queue->async_read_msg_with_timeout(5s, use_awaitable);
        
        if (ec == asio::error::timed_out) {
            // 5 秒内没有消息，执行定期任务
            perform_periodic_task();
            continue;
        }
        
        if (ec) break;
        
        // 处理接收到的消息
        process(msg);
    }
}
```

### 场景 3: 重试机制

```cpp
awaitable<std::optional<Message>> read_with_retry(auto queue, int max_retries) {
    for (int i = 0; i < max_retries; ++i) {
        auto [ec, msg] = co_await queue->async_read_msg_with_timeout(2s, use_awaitable);
        
        if (!ec) {
            co_return msg;  // 成功
        }
        
        if (ec != asio::error::timed_out) {
            co_return std::nullopt;  // 真正的错误
        }
        
        std::cout << "超时，重试 " << (i + 1) << "/" << max_retries << std::endl;
    }
    
    co_return std::nullopt;  // 所有重试都失败
}
```

### 场景 4: 批量处理优化

```cpp
awaitable<void> batch_processor(auto queue) {
    while (true) {
        // 等待最多 1 秒，收集最多 100 条消息
        auto [ec, messages] = co_await queue->async_read_msgs_with_timeout(100, 1s, use_awaitable);
        
        if (ec && ec != asio::error::timed_out) {
            break;  // 真正的错误
        }
        
        if (!messages.empty()) {
            // 批量处理
            process_batch(messages);
        } else {
            // 超时且无消息，可能需要做其他事情
            perform_idle_task();
        }
    }
}
```

### 场景 5: 心跳检测

```cpp
awaitable<void> heartbeat_monitor(auto queue) {
    const auto heartbeat_interval = 30s;
    
    while (true) {
        auto [ec, msg] = co_await queue->async_read_msg_with_timeout(heartbeat_interval, use_awaitable);
        
        if (ec == asio::error::timed_out) {
            std::cerr << "警告：" << heartbeat_interval.count() << "秒内未收到心跳！" << std::endl;
            on_heartbeat_timeout();
            continue;
        }
        
        if (ec) {
            break;
        }
        
        if (msg.type == MessageType::HEARTBEAT) {
            on_heartbeat_received();
        } else {
            process(msg);
        }
    }
}
```

### 场景 6: 超时后降级处理

```cpp
awaitable<void> resilient_processor(auto queue) {
    const auto timeout = 5s;
    int timeout_count = 0;
    
    while (true) {
        auto [ec, msg] = co_await queue->async_read_msg_with_timeout(timeout, use_awaitable);
        
        if (ec == asio::error::timed_out) {
            timeout_count++;
            
            if (timeout_count >= 3) {
                std::cout << "连续超时，切换到降级模式" << std::endl;
                use_fallback_mode();
                timeout_count = 0;
            }
            continue;
        }
        
        if (ec) break;
        
        // 成功接收消息，重置计数
        timeout_count = 0;
        process(msg);
    }
}
```

## 时间单位

支持所有 `std::chrono` 的时间单位：

```cpp
using namespace std::chrono_literals;

// 毫秒
co_await queue->async_read_msg_with_timeout(500ms, use_awaitable);

// 秒
co_await queue->async_read_msg_with_timeout(5s, use_awaitable);

// 分钟
co_await queue->async_read_msg_with_timeout(2min, use_awaitable);

// 小时
co_await queue->async_read_msg_with_timeout(1h, use_awaitable);

// 自定义
auto timeout = std::chrono::milliseconds(1500);
co_await queue->async_read_msg_with_timeout(timeout, use_awaitable);
```

## 错误处理

### 可能的错误码

| 错误码 | 说明 | 处理建议 |
|--------|------|----------|
| `std::error_code{}` | 成功 | 正常处理消息 |
| `asio::error::timed_out` | 超时 | 重试或执行降级逻辑 |
| `asio::error::operation_aborted` | 队列被停止 | 退出循环 |

### 完整错误处理示例

```cpp
awaitable<void> robust_reader(auto queue) {
    while (true) {
        auto [ec, msg] = co_await queue->async_read_msg_with_timeout(5s, use_awaitable);
        
        if (!ec) {
            // 成功
            try {
                process(msg);
            } catch (const std::exception& e) {
                std::cerr << "处理异常: " << e.what() << std::endl;
            }
            continue;
        }
        
        // 处理错误
        if (ec == asio::error::timed_out) {
            std::cout << "超时，继续等待..." << std::endl;
            continue;
        }
        
        if (ec == asio::error::operation_aborted) {
            std::cout << "队列已停止" << std::endl;
            break;
        }
        
        std::cerr << "未知错误: " << ec.message() << std::endl;
        break;
    }
}
```

## 性能考虑

### 1. 超时精度

超时使用 `asio::steady_timer`，精度取决于：
- 操作系统的定时器精度（通常 1-15ms）
- io_context 的运行频率

```cpp
// 短超时可能不太精确
co_await queue->async_read_msg_with_timeout(1ms, use_awaitable);  // 实际可能是 10-15ms

// 长超时精度更好
co_await queue->async_read_msg_with_timeout(1s, use_awaitable);   // 精度通常在几毫秒内
```

### 2. 资源使用

每个超时读取会创建一个 `steady_timer`：
- 内存开销：每个 timer 约 64-128 字节
- 如果有大量并发超时读取，考虑资源使用

```cpp
// ❌ 不推荐：创建大量并发超时操作
for (int i = 0; i < 1000; ++i) {
    co_spawn(io, [queue]() -> awaitable<void> {
        co_await queue->async_read_msg_with_timeout(10s, use_awaitable);
    }, detached);
}

// ✅ 推荐：使用合理数量的读取者
auto queue = disp->subscribe();
co_spawn(io, reader_task(queue), detached);  // 单个长期运行的读取者
```

### 3. 批量读取优化

使用批量超时读取可以提高效率：

```cpp
// ✅ 高效：批量处理
while (true) {
    auto [ec, msgs] = co_await queue->async_read_msgs_with_timeout(100, 1s, use_awaitable);
    if (!msgs.empty()) {
        process_batch(msgs);  // 一次处理多条
    }
}

// ❌ 低效：逐条处理
while (true) {
    auto [ec, msg] = co_await queue->async_read_msg_with_timeout(1s, use_awaitable);
    if (!ec) {
        process(msg);
    }
}
```

## 与旧 API 的对比

### 无超时（旧）

```cpp
// 会永久等待
auto [ec, msg] = co_await queue->async_read_msg(use_awaitable);
```

### 有超时（新）

```cpp
// 最多等待 5 秒
auto [ec, msg] = co_await queue->async_read_msg_with_timeout(5s, use_awaitable);
```

### 使用 awaitable_operators（另一种方式）

你仍然可以使用 `awaitable_operators` 实现超时，但内置方法更简洁：

```cpp
// 使用 operators（较繁琐）
using namespace asio::experimental::awaitable_operators;

asio::steady_timer timer(co_await asio::this_coro::executor);
timer.expires_after(5s);

auto result = co_await (
    queue->async_read_msg(use_awaitable) ||
    timer.async_wait(use_awaitable)
);

if (result.index() == 0) {
    auto [ec, msg] = std::get<0>(result);
    // 收到消息
} else {
    // 超时
}

// 使用内置方法（简洁）
auto [ec, msg] = co_await queue->async_read_msg_with_timeout(5s, use_awaitable);
if (ec == asio::error::timed_out) {
    // 超时
}
```

## 最佳实践

### ✅ 推荐

1. **总是检查错误码**
   ```cpp
   auto [ec, msg] = co_await queue->async_read_msg_with_timeout(5s, use_awaitable);
   if (ec) {
       // 处理错误
   }
   ```

2. **使用合适的超时值**
   ```cpp
   // 根据业务需求设置
   const auto timeout = 30s;  // 心跳检测
   const auto timeout = 100ms; // 快速响应
   ```

3. **实现降级策略**
   ```cpp
   if (ec == asio::error::timed_out) {
       use_cached_data();  // 降级到缓存
   }
   ```

### ❌ 避免

1. **不检查错误**
   ```cpp
   // 错误：忽略错误码
   auto [ec, msg] = co_await queue->async_read_msg_with_timeout(5s, use_awaitable);
   process(msg);  // msg 可能无效！
   ```

2. **过短的超时**
   ```cpp
   // 可能导致频繁超时
   co_await queue->async_read_msg_with_timeout(1ms, use_awaitable);
   ```

3. **忘记处理超时**
   ```cpp
   // 超时后应该做点什么
   if (ec == asio::error::timed_out) {
       // 不要留空！
   }
   ```

## 运行示例

```bash
cd src/bcast

# 编译
g++ -std=c++20 -fcoroutines timeout_example.cpp -lpthread -o timeout_example

# 运行
./timeout_example
```

示例展示了 5 种不同的超时使用模式。

## 总结

超时功能让你能够：
- ✅ 防止无限等待
- ✅ 实现定期任务
- ✅ 构建重试机制
- ✅ 实现心跳检测
- ✅ 优雅降级

使用超时可以让你的应用更加健壮和可靠！

