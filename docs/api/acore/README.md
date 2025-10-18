# acore - Asynchronous Publish-Subscribe Pattern

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
#include "acore/dispatcher.hpp"
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
    auto dispatcher = acore::make_dispatcher<std::string>(io);
    
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
auto dispatcher = acore::make_dispatcher<Message>(io_context);

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

### async_waitgroup - 异步等待组

类似 Go 的 `sync.WaitGroup`，用于等待一组异步任务完成。

```cpp
auto wg = std::make_shared<acore::async_waitgroup>(io_context.get_executor());

// 启动 3 个异步任务
wg->add(3);
for (int i = 0; i < 3; ++i) {
    asio::co_spawn(io_context, [wg, i]() -> asio::awaitable<void> {
        co_await do_async_work(i);
        wg->done();  // 完成一个任务
    }, asio::detached);
}

// 等待所有任务完成
co_await wg->wait(asio::use_awaitable);
std::cout << "所有任务完成！\n";

// 支持超时等待
bool completed = co_await wg->wait_for(30s, asio::use_awaitable);
```

**详细文档**: 见 [WAITGROUP_USAGE.md](WAITGROUP_USAGE.md)

### async_semaphore - 异步信号量

用于控制并发访问数量。

```cpp
auto sem = std::make_shared<acore::async_semaphore>(ex, 3);  // 最多 3 个并发

// 获取信号量
co_await sem->acquire(asio::use_awaitable);
// ... 使用资源 ...
sem->release();  // 释放

// 取消支持
uint64_t id = sem->acquire_cancellable([](){ /* callback */ });
sem->cancel(id);  // 取消等待
```

### async_event - 异步事件

手动重置事件，用于广播通知。

```cpp
auto event = std::make_shared<acore::async_event>(ex);

// 等待事件触发
co_await event->wait(asio::use_awaitable);

// 触发事件（唤醒所有等待者）
event->notify_all();

// 重置事件
event->reset();
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
add_subdirectory(src/acore)
target_link_libraries(your_target PRIVATE acore)
```

### 直接编译

```bash
g++ -std=c++20 -fcoroutines your_code.cpp -lpthread -o app
```

## 文档

- **[完整使用指南](../../docs/acore/acore_GUIDE.md)** - 详细的 API 文档和最佳实践
- **[批量操作](../../docs/acore/BATCH_OPERATIONS.md)** - 批量操作详解
- **[超时功能](../../docs/acore/TIMEOUT_FEATURES.md)** - 超时机制详解

## 示例代码

查看 `examples/acore/` 目录：

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

### dispatcher + async_queue
- 实时消息系统
- 事件驱动架构
- 微服务通信
- WebSocket 广播
- 日志聚合
- 数据流处理

### async_waitgroup
- 等待多个异步任务完成
- 优雅关闭服务器（等待请求处理完毕）
- 批量操作协调（如批量下载、批量处理）
- Worker Pool 生命周期管理
- 分阶段任务流水线

### async_semaphore
- 限制并发数（如连接池、线程池）
- 资源访问控制
- 流量控制和背压

### async_event
- 状态变化通知
- 多订阅者事件广播
- 条件同步

## License

与项目主许可证相同。
