# ACORE - Asio Core Components

异步核心组件库，提供基于 Boost.Asio 的高级异步原语。

## 组件概览

### 📦 核心组件

| 组件 | 文件 | 说明 |
|------|------|------|
| **Async Semaphore** | `async_semaphore.hpp` | 异步信号量，支持协程 |
| **Async Queue** | `async_queue.hpp` | 线程安全的异步队列 |
| **Async Event** | `async_event.hpp` | 异步事件通知机制 |
| **Async WaitGroup** | `async_waitgroup.hpp` | 类似 Go 的 WaitGroup |
| **Dispatcher** | `dispatcher.hpp` | 任务调度器 |
| **Handler Traits** | `handler_traits.hpp` | 处理器类型萃取 |

## 📖 文档

完整的 API 文档和使用指南位于：
- [ACORE API 文档](/docs/api/acore/)
- [异步原语说明](/docs/api/acore/ASYNC_PRIMITIVES.md)
- [取消机制详解](/docs/api/acore/CANCELLATION_SUPPORT.md)
- [WaitGroup 用法](/docs/api/acore/WAITGROUP_USAGE.md)

## 🚀 快速示例

### 异步信号量
```cpp
#include "acore/async_semaphore.hpp"

asio::awaitable<void> example(asio::io_context& io) {
    acore::async_semaphore sem(io, 3); // 最多3个并发
    
    co_await sem.async_acquire(asio::use_awaitable);
    // 执行受保护的操作
    sem.release();
}
```

### 异步队列
```cpp
#include "acore/async_queue.hpp"

asio::awaitable<void> consumer(acore::async_queue<int>& queue) {
    auto value = co_await queue.async_pop(asio::use_awaitable);
    // 处理 value
}
```

### WaitGroup
```cpp
#include "acore/async_waitgroup.hpp"

asio::awaitable<void> parallel_tasks() {
    acore::async_waitgroup wg(co_await asio::this_coro::executor);
    
    for (int i = 0; i < 10; ++i) {
        wg.add(1);
        asio::co_spawn(/* ... */, asio::detached);
    }
    
    co_await wg.wait(asio::use_awaitable);
}
```

## 🔧 编译

ACORE 是纯头文件库，无需单独编译。只需包含相应的头文件即可使用。

依赖：
- Boost.Asio (>= 1.70)
- C++20 (协程支持)

## 🧪 测试

```bash
cd src/acore
./build_test.sh          # 构建所有测试
./test_cancellation      # 运行取消机制测试
./test_waitgroup         # 运行 WaitGroup 测试
```

## 📝 注意事项

- 所有组件都支持协程（C++20）
- 支持 Asio 的取消机制（cancellation slots）
- 线程安全的组件会在文档中明确标注
- 建议使用 `use_awaitable` 获得最佳开发体验
