# ACORE - 异步核心组件 API

ACORE (Asio Core) 是基于 Boost.Asio 构建的高级异步原语库，提供了一组强大的并发编程工具。

## 📦 组件列表

| 组件 | 头文件 | 说明 |
|------|--------|------|
| **Async Semaphore** | `async_semaphore.hpp` | 异步信号量，控制并发访问 |
| **Async Queue** | `async_queue.hpp` | 线程安全的异步队列 |
| **Async Event** | `async_event.hpp` | 异步事件通知机制 |
| **Async WaitGroup** | `async_waitgroup.hpp` | 类似 Go 的 WaitGroup，等待一组操作完成 |
| **Dispatcher** | `dispatcher.hpp` | 任务调度器 |
| **Handler Traits** | `handler_traits.hpp` | 处理器类型萃取工具 |

## 📖 核心文档

### 入门文档
- **[异步原语总览](ASYNC_PRIMITIVES.md)** - 所有组件的使用说明
- **[协程模式](COROUTINE_ONLY.md)** - C++20 协程 API 说明
- **[取消机制](CANCELLATION_SUPPORT.md)** - 操作取消的详细说明
- **[WaitGroup 用法](WAITGROUP_USAGE.md)** - WaitGroup 详细使用指南

### 高级分析
- **[Event 组件分析](ASYNC_EVENT_ANALYSIS.md)** - Event 实现分析
- **[Event 重构说明](ASYNC_EVENT_REFACTORED.md)** - Event 重构文档
- **[Queue 简化说明](ASYNC_QUEUE_SIMPLIFICATION.md)** - Queue 简化设计
- **[Semaphore 详解](ASYNC_SEMAPHORE_EXPLAINED.md)** - Semaphore 深度解析

## 🚀 快速开始

### 异步信号量（控制并发）

```cpp
#include "acore/async_semaphore.hpp"
#include <asio/co_spawn.hpp>
#include <asio/use_awaitable.hpp>

asio::awaitable<void> worker(acore::async_semaphore& sem, int id) {
    // 获取信号量（最多允许 N 个并发）
    co_await sem.async_acquire(asio::use_awaitable);
    
    std::cout << "Worker " << id << " is working\n";
    co_await asio::steady_timer(
        co_await asio::this_coro::executor, 
        std::chrono::seconds(1)
    ).async_wait(asio::use_awaitable);
    
    // 释放信号量
    sem.release();
    std::cout << "Worker " << id << " finished\n";
}

int main() {
    asio::io_context io;
    acore::async_semaphore sem(io, 3); // 最多 3 个并发
    
    // 启动 10 个工作协程（但同时只有 3 个在运行）
    for (int i = 0; i < 10; ++i) {
        asio::co_spawn(io, worker(sem, i), asio::detached);
    }
    
    io.run();
}
```

### 异步队列（生产者-消费者）

```cpp
#include "acore/async_queue.hpp"

asio::awaitable<void> producer(acore::async_queue<int>& queue) {
    for (int i = 0; i < 10; ++i) {
        queue.push(i);  // 线程安全的推送
        co_await asio::steady_timer(
            co_await asio::this_coro::executor,
            std::chrono::milliseconds(100)
        ).async_wait(asio::use_awaitable);
    }
    queue.close();  // 关闭队列
}

asio::awaitable<void> consumer(acore::async_queue<int>& queue) {
    while (true) {
        try {
            auto value = co_await queue.async_pop(asio::use_awaitable);
            std::cout << "Got: " << value << "\n";
        } catch (const std::runtime_error&) {
            break;  // 队列已关闭且为空
        }
    }
}

int main() {
    asio::io_context io;
    acore::async_queue<int> queue(io);
    
    asio::co_spawn(io, producer(queue), asio::detached);
    asio::co_spawn(io, consumer(queue), asio::detached);
    
    io.run();
}
```

### 异步事件（通知机制）

```cpp
#include "acore/async_event.hpp"

asio::awaitable<void> waiter(acore::async_event& event, int id) {
    std::cout << "Waiter " << id << " is waiting...\n";
    co_await event.async_wait(asio::use_awaitable);
    std::cout << "Waiter " << id << " received signal!\n";
}

int main() {
    asio::io_context io;
    acore::async_event event(io);
    
    // 启动多个等待者
    for (int i = 0; i < 5; ++i) {
        asio::co_spawn(io, waiter(event, i), asio::detached);
    }
    
    // 2秒后触发事件
    asio::steady_timer timer(io, std::chrono::seconds(2));
    timer.async_wait([&](auto) {
        event.set();  // 唤醒所有等待者
    });
    
    io.run();
}
```

### WaitGroup（等待一组任务）

```cpp
#include "acore/async_waitgroup.hpp"

asio::awaitable<void> task(acore::async_waitgroup& wg, int id) {
    std::cout << "Task " << id << " started\n";
    
    co_await asio::steady_timer(
        co_await asio::this_coro::executor,
        std::chrono::seconds(1)
    ).async_wait(asio::use_awaitable);
    
    std::cout << "Task " << id << " done\n";
    wg.done();  // 标记任务完成
}

asio::awaitable<void> coordinator() {
    auto ex = co_await asio::this_coro::executor;
    acore::async_waitgroup wg(ex);
    
    // 启动 5 个任务
    for (int i = 0; i < 5; ++i) {
        wg.add(1);  // 增加计数
        asio::co_spawn(ex, task(wg, i), asio::detached);
    }
    
    // 等待所有任务完成
    co_await wg.wait(asio::use_awaitable);
    std::cout << "All tasks completed!\n";
}

int main() {
    asio::io_context io;
    asio::co_spawn(io, coordinator(), asio::detached);
    io.run();
}
```

## ✨ 主要特性

### 1. C++20 协程原生支持
所有组件都支持 `co_await`，提供现代化的异步编程体验。

### 2. 操作取消支持
所有异步操作都支持 Asio 的取消机制（cancellation slots）。

```cpp
asio::cancellation_signal sig;
auto token = asio::bind_cancellation_slot(
    sig.slot(),
    asio::use_awaitable
);

// 在其他地方取消
sig.emit(asio::cancellation_type::all);
```

详见：[取消机制文档](CANCELLATION_SUPPORT.md)

### 3. 线程安全
- `async_queue` - 完全线程安全
- `async_semaphore` - 完全线程安全
- `async_event` - 完全线程安全
- `async_waitgroup` - 完全线程安全

### 4. 零拷贝和移动语义
所有组件都优化了移动语义，支持高效的对象传递。

### 5. 头文件库
ACORE 是纯头文件库，只需包含相应的头文件即可使用。

## 📝 使用要求

- **C++ 标准**: C++20（协程支持）
- **依赖库**: Boost.Asio >= 1.70 或独立 ASIO >= 1.18
- **编译器**: 
  - GCC 10+
  - Clang 12+
  - MSVC 2019+

## 🧪 测试

每个组件都有对应的测试程序：

```bash
cd src/acore

# 编译所有测试
./build_all_tests.sh

# 运行所有测试
./run_all_tests.sh

# 或单独运行
./test_async_semaphore
./test_async_queue
./test_async_event
./test_waitgroup
```

## 📚 详细文档

- **[ASYNC_PRIMITIVES.md](ASYNC_PRIMITIVES.md)** - 所有组件的详细 API 文档
- **[CANCELLATION_SUPPORT.md](CANCELLATION_SUPPORT.md)** - 取消机制详解
- **[COROUTINE_ONLY.md](COROUTINE_ONLY.md)** - 协程专用 API 说明
- **[WAITGROUP_USAGE.md](WAITGROUP_USAGE.md)** - WaitGroup 完整指南

## 💡 最佳实践

1. **优先使用 `use_awaitable`** - 在协程中获得最佳体验
2. **合理使用取消机制** - 避免资源泄漏
3. **注意 WaitGroup 的 add/done 配对** - 类似 Go 的规则
4. **Queue 使用完毕记得 close()** - 通知消费者结束
5. **Semaphore 的 acquire/release 要配对** - 或使用 RAII

## 🔗 相关链接

- [源代码](../../../src/acore/) - ACORE 源码目录
- [示例代码](../../../examples/acore/) - 实际使用示例
- [设计文档](../../design/) - 设计决策文档
- [项目主页](../../../README.md) - ASIO-SRT 项目主页
