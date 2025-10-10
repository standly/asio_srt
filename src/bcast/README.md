# Asynchronous Publish-Subscribe Pattern

基于 ASIO strand 实现的异步、无锁发布订阅模式，支持 **C++20 协程**。

## ⚡ 新特性：协程支持

**最简单的发布订阅 API：**

```cpp
// 发布
dispatcher->publish(message);

// 订阅
auto queue = dispatcher->subscribe();
auto [ec, msg] = co_await queue->async_read_msg(use_awaitable);
```

用同步的方式写异步代码，就是这么简单！

## 📚 文档导航

- 🚀 **[快速开始.md](快速开始.md)** - 5 分钟上手指南
- 📖 **[协程API指南.md](协程API指南.md)** - 详细的协程 API 文档
- ⏱️ **[超时功能指南.md](超时功能指南.md)** - 超时读取功能详解
- 🔄 **[API对比.md](API对比.md)** - 回调 vs 协程对比
- 📘 **[使用指南.md](使用指南.md)** - 完整使用指南（中文）
- 💻 **示例代码**：
  - `coroutine_example.cpp` - 协程基础示例
  - `timeout_example.cpp` - 超时功能示例 ⭐
  - `real_world_example.cpp` - 实战场景（聊天室、股票）
  - `example.cpp` - 回调方式示例
  - `advanced_example.cpp` - 高级模式
  - `benchmark.cpp` - 性能测试

## 核心组件

### 1. `async_queue<T>`
异步消息队列，使用 ASIO strand 实现线程安全的无锁操作。

**特性：**
- 使用 strand 序列化所有操作，无需显式锁
- 支持协程（C++20）和回调两种接口
- 异步消息处理，不阻塞
- 支持单条和批量读取

**协程接口：**
```cpp
// 读取单条消息
auto [ec, msg] = co_await queue->async_read_msg(use_awaitable);

// 读取多条消息（最多 max_count 条）
auto [ec, messages] = co_await queue->async_read_msgs(max_count, use_awaitable);

// 带超时的读取 ⭐ 新功能
auto [ec, msg] = co_await queue->async_read_msg_with_timeout(5s, use_awaitable);
auto [ec, messages] = co_await queue->async_read_msgs_with_timeout(10, 5s, use_awaitable);

// 控制方法
void stop();              // 停止队列
bool is_stopped() const;  // 检查是否停止
size_t size() const;      // 获取队列大小
```

### 2. `dispatcher<T>`
消息分发器，管理多个订阅者并广播消息。

**特性：**
- 每个订阅者有独立的 async_queue
- 线程安全的订阅/取消订阅
- 异步消息分发，不阻塞发布者
- 自动管理订阅者生命周期

**主要方法：**
```cpp
// 订阅 - 返回一个队列
std::shared_ptr<async_queue<T>> subscribe();

// 发布消息（线程安全，可从任意线程调用）
void publish(const T& msg);  // 复制版本
void publish(T&& msg);       // 移动版本

// 取消订阅
void unsubscribe(std::shared_ptr<async_queue<T>> queue);

// 其他
void clear();                    // 清空所有订阅者
size_t subscriber_count() const; // 获取订阅者数量
```

## 设计原理

### 无锁设计
使用 ASIO `strand` 实现无锁并发：
- `strand` 确保所有操作按顺序在单个线程上执行
- 避免了显式的 mutex 和条件变量
- 消除了竞态条件和死锁风险

### 异步架构
- 所有操作通过 `asio::post()` 异步执行
- 消息处理不会阻塞发布者
- 每个订阅者独立处理消息，互不影响

### 线程安全
- `publish()` 可以从任何线程调用
- `subscribe()` / `unsubscribe()` 可以从任何线程调用
- 通过 strand 序列化确保数据一致性

## 使用示例

### 基本用法（协程版本 - 推荐）

```cpp
#include "dispatcher.hpp"
#include <asio.hpp>
#include <asio/co_spawn.hpp>
#include <asio/use_awaitable.hpp>

using asio::awaitable;
using asio::co_spawn;
using asio::detached;
using asio::use_awaitable;

awaitable<void> subscriber_task(std::shared_ptr<bcast::async_queue<std::string>> queue) {
    while (true) {
        // 读取消息 - 简单直观！
        auto [ec, msg] = co_await queue->async_read_msg(use_awaitable);
        if (ec) break;
        
        std::cout << "Received: " << msg << std::endl;
    }
}

int main() {
    asio::io_context io_context;
    
    // 1. 创建 dispatcher
    auto dispatcher = bcast::make_dispatcher<std::string>(io_context);
    
    // 2. 订阅 - 获取队列
    auto queue = dispatcher->subscribe();
    
    // 3. 启动订阅者协程
    co_spawn(io_context, subscriber_task(queue), detached);
    
    // 4. 发布消息
    dispatcher->publish("Hello, World!");
    dispatcher->publish("Hello, Coroutines!");
    
    // 5. 运行
    io_context.run();
    
    return 0;
}
```

### 批量读取示例

```cpp
awaitable<void> batch_subscriber(auto queue) {
    while (true) {
        // 一次读取最多 10 条消息
        auto [ec, messages] = co_await queue->async_read_msgs(10, use_awaitable);
        if (ec) break;
        
        std::cout << "Batch of " << messages.size() << " messages:" << std::endl;
        for (const auto& msg : messages) {
            process(msg);
        }
    }
}
```

### 超时读取示例 ⭐

```cpp
using namespace std::chrono_literals;

awaitable<void> timeout_subscriber(auto queue) {
    while (true) {
        // 最多等待 5 秒
        auto [ec, msg] = co_await queue->async_read_msg_with_timeout(5s, use_awaitable);
        
        if (ec == asio::error::timed_out) {
            std::cout << "超时，执行定期任务..." << std::endl;
            perform_periodic_task();
            continue;
        }
        
        if (ec) break;
        
        std::cout << "Received: " << msg << std::endl;
    }
}
```

### 多线程环境

```cpp
asio::io_context io_context;
auto dispatcher = bcast::make_dispatcher<int>(io_context);

// 订阅者
auto sub = dispatcher->subscribe([](int value) {
    std::cout << "Thread " << std::this_thread::get_id() 
              << " received: " << value << std::endl;
});

// 多个 io_context 运行线程
std::vector<std::thread> threads;
for (int i = 0; i < 4; ++i) {
    threads.emplace_back([&io_context]() {
        io_context.run();
    });
}

// 从多个线程发布消息（线程安全）
std::thread publisher1([&]() {
    for (int i = 0; i < 10; ++i) {
        dispatcher->publish(i);
    }
});

std::thread publisher2([&]() {
    for (int i = 100; i < 110; ++i) {
        dispatcher->publish(i);
    }
});

// 等待完成
publisher1.join();
publisher2.join();

// 停止并清理
io_context.stop();
for (auto& t : threads) {
    t.join();
}
```

### 自定义消息类型

```cpp
struct Event {
    std::string type;
    std::string data;
    int timestamp;
};

asio::io_context io_context;
auto dispatcher = bcast::make_dispatcher<Event>(io_context);

// 订阅特定类型的事件
auto sub = dispatcher->subscribe([](const Event& event) {
    if (event.type == "important") {
        std::cout << "Important event: " << event.data << std::endl;
    }
});

// 发布事件
dispatcher->publish(Event{"important", "System update", 12345});
```

## 性能特点

1. **零锁开销**：使用 strand 避免锁竞争
2. **异步处理**：发布者不会被慢速订阅者阻塞
3. **可扩展性**：支持大量订阅者
4. **低延迟**：消息通过队列快速分发

## 典型应用场景

- 实时消息广播
- 事件驱动架构
- 数据流处理
- 微服务间通信
- 日志聚合
- 传感器数据分发
- WebSocket 消息分发

## 编译

### 作为头文件库
```cmake
add_subdirectory(src/bcast)
target_link_libraries(your_target PRIVATE bcast)
```

### 编译示例
```bash
# 确保安装了 ASIO
cd src/bcast
g++ -std=c++17 example.cpp -lpthread -o example
./example
```

## 注意事项

1. **io_context 必须运行**：确保 `io_context.run()` 在某个线程中执行
2. **生命周期管理**：dispatcher 和订阅者必须在 io_context 停止前有效
3. **消息复制**：每个订阅者接收消息的副本，考虑使用 `shared_ptr` 优化大消息
4. **异常处理**：可以为 async_queue 设置错误处理器捕获异常

## 高级技巧

### 使用 shared_ptr 优化大消息

```cpp
using MessagePtr = std::shared_ptr<LargeMessage>;
auto dispatcher = bcast::make_dispatcher<MessagePtr>(io_context);

auto msg = std::make_shared<LargeMessage>(...);
dispatcher->publish(msg);  // 只复制指针，不复制数据
```

### 过滤和转换

```cpp
// 订阅者可以过滤消息
dispatcher->subscribe([](const Message& msg) {
    if (msg.priority > 5) {
        // 只处理高优先级消息
        process(msg);
    }
});
```

### 级联 Dispatcher

```cpp
// 创建多级消息路由
auto main_dispatcher = bcast::make_dispatcher<Event>(io_context);
auto filtered_dispatcher = bcast::make_dispatcher<Event>(io_context);

main_dispatcher->subscribe([&](const Event& event) {
    if (event.category == "important") {
        filtered_dispatcher->publish(event);
    }
});
```

## License

与项目主许可证相同。

