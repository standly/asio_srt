# BCAST - Asynchronous Publish-Subscribe Pattern

基于 ASIO strand 实现的异步、无锁发布订阅模式，支持 **C++20 协程**。

## 核心特性

- ⚡ **无锁设计**：使用 ASIO strand，零锁开销
- 🔄 **协程支持**：用同步的方式写异步代码
- 🚀 **高性能**：微秒级延迟，10万+ 消息/秒
- 🔒 **线程安全**：所有 API 都可从任意线程调用
- 📦 **批量操作**：批量 push/publish，性能提升 10-100 倍
- ⏱️ **超时支持**：防止无限等待

## 快速示例

```cpp
#include "bcast/dispatcher.hpp"
#include <asio.hpp>
#include <asio/co_spawn.hpp>
#include <asio/use_awaitable.hpp>

using asio::awaitable;
using asio::co_spawn;
using asio::detached;
using asio::use_awaitable;

awaitable<void> subscriber(auto queue) {
    while (true) {
        auto [ec, msg] = co_await queue->async_read_msg(use_awaitable);
        if (ec) break;
        std::cout << "收到: " << msg << std::endl;
    }
}

int main() {
    asio::io_context io;
    auto dispatcher = bcast::make_dispatcher<std::string>(io);
    
    auto queue = dispatcher->subscribe();
    co_spawn(io, subscriber(queue), detached);
    
    dispatcher->publish("Hello, World!");
    
    io.run();
    return 0;
}
```

## 核心组件

### dispatcher<T> - 消息分发器

```cpp
auto dispatcher = bcast::make_dispatcher<Message>(io_context);

// 订阅
auto queue = dispatcher->subscribe();

// 发布
dispatcher->publish(message);
dispatcher->publish_batch({msg1, msg2, msg3});  // 批量发布

// 管理
dispatcher->unsubscribe(queue);
dispatcher->clear();
```

### async_queue<T> - 异步消息队列

```cpp
// 读取消息
auto [ec, msg] = co_await queue->async_read_msg(use_awaitable);
auto [ec, msgs] = co_await queue->async_read_msgs(10, use_awaitable);

// 带超时的读取
auto [ec, msg] = co_await queue->async_read_msg_with_timeout(5s, use_awaitable);

// 推送消息
queue->push(message);
queue->push_batch({msg1, msg2, msg3});  // 批量推送
```

## 主要特性

### 1. 协程接口

用同步的方式写异步代码：

```cpp
while (true) {
    auto [ec, msg] = co_await queue->async_read_msg(use_awaitable);
    if (ec) break;
    process(msg);
}
```

### 2. 超时支持

防止无限等待：

```cpp
using namespace std::chrono_literals;

auto [ec, msg] = co_await queue->async_read_msg_with_timeout(5s, use_awaitable);
if (ec == asio::error::timed_out) {
    handle_timeout();
}
```

### 3. 批量操作

显著提升性能（10-100倍）：

```cpp
// 批量发布
std::vector<Message> batch = {msg1, msg2, msg3};
dispatcher->publish_batch(batch);

// 批量读取
auto [ec, messages] = co_await queue->async_read_msgs(100, use_awaitable);
```

## 编译要求

- **C++20** 或更高（协程支持）
- ASIO (standalone 或 Boost.Asio)
- 支持协程的编译器：GCC 10+, Clang 10+, MSVC 2019 16.8+

## 集成

### CMake

```cmake
add_subdirectory(src/bcast)
target_link_libraries(your_target PRIVATE bcast)
```

### 直接编译

```bash
g++ -std=c++20 -fcoroutines your_code.cpp -lpthread -o app
```

## 文档

- **[完整使用指南](../../docs/bcast/BCAST_GUIDE.md)** - 详细的 API 文档和最佳实践
- **[批量操作](../../docs/bcast/BATCH_OPERATIONS.md)** - 批量操作详解
- **[超时功能](../../docs/bcast/TIMEOUT_FEATURES.md)** - 超时机制详解

## 示例代码

查看 `examples/bcast/` 目录：

- `coroutine_example.cpp` - 协程基础示例
- `timeout_example.cpp` - 超时功能示例
- `batch_example.cpp` - 批量操作示例
- `real_world_example.cpp` - 实战场景（聊天室、股票）
- `benchmark.cpp` - 性能测试

## 设计原理

### 无锁设计

使用 ASIO `strand` 序列化操作，避免锁：

```cpp
asio::post(strand_, [self = this->shared_from_this()]() {
    // 所有操作在 strand 上串行执行
    // 天然线程安全，无需锁
});
```

### 每订阅者独立队列

```
发布者                     订阅者队列
         ┌──> Queue 1 ──> 订阅者 1
Publish ─┼──> Queue 2 ──> 订阅者 2
         └──> Queue 3 ──> 订阅者 3
```

订阅者之间完全隔离，互不影响。

## 性能特征

- **吞吐量**：100,000+ 消息/秒
- **延迟**：< 100 微秒（平均）
- **可扩展性**：支持 100+ 订阅者
- **批量加速**：10-100 倍性能提升

## 适用场景

- 实时消息系统
- 事件驱动架构
- 微服务通信
- WebSocket 广播
- 日志聚合
- 数据流处理

## License

与项目主许可证相同。
