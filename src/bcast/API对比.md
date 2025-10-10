# API 对比：回调 vs 协程

## 核心设计哲学

### 旧设计（回调）
```
订阅时指定回调 → 每次消息触发回调
```

### 新设计（协程）
```
订阅获取队列 → 用 co_await 按需读取
```

## 使用对比

### 简单消息处理

#### 回调方式 ❌
```cpp
auto sub_id = dispatcher->subscribe([](const Message& msg) {
    std::cout << msg.content << std::endl;
});

// 问题：
// - 必须在订阅时提供回调
// - 难以添加控制流逻辑
// - 无法暂停/恢复
```

#### 协程方式 ✅
```cpp
auto queue = dispatcher->subscribe();

co_spawn(executor, [queue]() -> awaitable<void> {
    while (true) {
        auto [ec, msg] = co_await queue->async_read_msg(use_awaitable);
        if (ec) break;
        
        std::cout << msg.content << std::endl;
    }
}, detached);

// 优势：
// ✓ 代码更直观（像同步代码）
// ✓ 容易添加逻辑（if/for/while）
// ✓ 可以暂停和恢复
```

### 带条件的消息处理

#### 回调方式 ❌
```cpp
int count = 0;
auto sub_id = dispatcher->subscribe([&count](const Message& msg) {
    count++;
    if (count > 10) {
        // 怎么停止？需要额外的标志和检查
        return;
    }
    process(msg);
});
```

#### 协程方式 ✅
```cpp
auto queue = dispatcher->subscribe();

co_spawn(executor, [queue]() -> awaitable<void> {
    for (int count = 0; count < 10; ++count) {
        auto [ec, msg] = co_await queue->async_read_msg(use_awaitable);
        if (ec) break;
        
        process(msg);
    }
    // 自然退出，自动清理
}, detached);
```

### 异步操作链

#### 回调方式 ❌（回调地狱）
```cpp
dispatcher->subscribe([](const Message& msg) {
    async_operation1(msg, [](Result1 res1) {
        async_operation2(res1, [](Result2 res2) {
            async_operation3(res2, [](Result3 res3) {
                // 嵌套太深...
            });
        });
    });
});
```

#### 协程方式 ✅（扁平化）
```cpp
auto queue = dispatcher->subscribe();

co_spawn(executor, [queue]() -> awaitable<void> {
    while (true) {
        auto [ec, msg] = co_await queue->async_read_msg(use_awaitable);
        if (ec) break;
        
        auto res1 = co_await async_operation1(msg, use_awaitable);
        auto res2 = co_await async_operation2(res1, use_awaitable);
        auto res3 = co_await async_operation3(res2, use_awaitable);
        // 清晰的顺序流程
    }
}, detached);
```

### 错误处理

#### 回调方式 ❌
```cpp
dispatcher->subscribe([](const Message& msg) {
    try {
        process(msg);
    } catch (const std::exception& e) {
        // 只能捕获同步异常
        // 异步操作的错误需要额外的错误回调
    }
});
```

#### 协程方式 ✅
```cpp
auto queue = dispatcher->subscribe();

co_spawn(executor, [queue]() -> awaitable<void> {
    try {
        while (true) {
            auto [ec, msg] = co_await queue->async_read_msg(use_awaitable);
            if (ec) {
                handle_error(ec);
                break;
            }
            
            process(msg);
        }
    } catch (const std::exception& e) {
        // 统一的异常处理
        log_error(e.what());
    }
}, detached);
```

### 状态管理

#### 回调方式 ❌
```cpp
struct State {
    int phase = 0;
    std::vector<Message> buffer;
};

auto state = std::make_shared<State>();

dispatcher->subscribe([state](const Message& msg) {
    // 状态逻辑分散在回调中，难以理解
    if (state->phase == 0) {
        state->buffer.push_back(msg);
        if (state->buffer.size() >= 10) {
            state->phase = 1;
        }
    } else if (state->phase == 1) {
        // ...
    }
});
```

#### 协程方式 ✅
```cpp
auto queue = dispatcher->subscribe();

co_spawn(executor, [queue]() -> awaitable<void> {
    // 状态作为本地变量，清晰直观
    std::vector<Message> buffer;
    
    // Phase 1: 收集 10 条消息
    for (int i = 0; i < 10; ++i) {
        auto [ec, msg] = co_await queue->async_read_msg(use_awaitable);
        if (ec) co_return;
        buffer.push_back(msg);
    }
    
    // Phase 2: 处理
    process_batch(buffer);
    
    // Phase 3: 继续...
    while (true) {
        auto [ec, msg] = co_await queue->async_read_msg(use_awaitable);
        if (ec) break;
        process(msg);
    }
}, detached);
```

### 多个订阅者

#### 回调方式
```cpp
// 每个订阅者独立的回调
auto sub1 = dispatcher->subscribe([](const Message& msg) {
    process1(msg);
});

auto sub2 = dispatcher->subscribe([](const Message& msg) {
    process2(msg);
});

auto sub3 = dispatcher->subscribe([](const Message& msg) {
    process3(msg);
});

// 取消订阅
dispatcher->unsubscribe(sub1);
dispatcher->unsubscribe(sub2);
dispatcher->unsubscribe(sub3);
```

#### 协程方式
```cpp
// 每个订阅者获取自己的队列
auto queue1 = dispatcher->subscribe();
auto queue2 = dispatcher->subscribe();
auto queue3 = dispatcher->subscribe();

// 启动订阅者协程
co_spawn(executor, subscriber_task(queue1, "Sub1"), detached);
co_spawn(executor, subscriber_task(queue2, "Sub2"), detached);
co_spawn(executor, subscriber_task(queue3, "Sub3"), detached);

// 取消订阅
dispatcher->unsubscribe(queue1);
dispatcher->unsubscribe(queue2);
dispatcher->unsubscribe(queue3);
```

### 批量处理

#### 回调方式 ❌（复杂）
```cpp
std::vector<Message> batch;

dispatcher->subscribe([&batch](const Message& msg) {
    batch.push_back(msg);
    
    if (batch.size() >= 100) {
        process_batch(batch);
        batch.clear();
    }
    
    // 问题：如果消息不足 100 条怎么办？需要定时器...
});
```

#### 协程方式 ✅（简单）
```cpp
auto queue = dispatcher->subscribe();

co_spawn(executor, [queue]() -> awaitable<void> {
    while (true) {
        // 直接读取最多 100 条消息
        auto [ec, messages] = co_await queue->async_read_msgs(100, use_awaitable);
        if (ec) break;
        
        if (!messages.empty()) {
            process_batch(messages);
        }
    }
}, detached);
```

### 超时处理

#### 回调方式 ❌（需要额外的定时器逻辑）
```cpp
asio::steady_timer timer(io_context);
bool received = false;

dispatcher->subscribe([&](const Message& msg) {
    received = true;
    timer.cancel();
    process(msg);
});

timer.expires_after(5s);
timer.async_wait([&](std::error_code ec) {
    if (!ec && !received) {
        // 超时
    }
});
```

#### 协程方式 ✅（使用 || 运算符）
```cpp
auto queue = dispatcher->subscribe();

co_spawn(executor, [queue]() -> awaitable<void> {
    using namespace asio::experimental::awaitable_operators;
    
    asio::steady_timer timer(co_await asio::this_coro::executor);
    timer.expires_after(5s);
    
    // 等待消息或超时
    auto result = co_await (
        queue->async_read_msg(use_awaitable) ||
        timer.async_wait(use_awaitable)
    );
    
    if (result.index() == 0) {
        auto [ec, msg] = std::get<0>(result);
        process(msg);
    } else {
        handle_timeout();
    }
}, detached);
```

## 发布消息

两种方式相同，都很简单：

```cpp
// 复制语义
dispatcher->publish(message);

// 移动语义
dispatcher->publish(std::move(message));
```

## 性能对比

| 特性 | 回调方式 | 协程方式 |
|------|----------|----------|
| 运行时开销 | ⚡ 极低 | ⚡ 极低 |
| 内存占用 | 📊 小 | 📊 略大（协程栈） |
| 代码可读性 | ⭐⭐ | ⭐⭐⭐⭐⭐ |
| 维护性 | ⭐⭐ | ⭐⭐⭐⭐⭐ |
| 控制流 | ⭐⭐ | ⭐⭐⭐⭐⭐ |
| 错误处理 | ⭐⭐ | ⭐⭐⭐⭐⭐ |
| 学习曲线 | ⭐⭐⭐ | ⭐⭐⭐⭐ |

## 何时使用哪种方式？

### 使用回调方式当：
- ✓ 处理逻辑非常简单（单行代码）
- ✓ 不需要复杂的控制流
- ✓ 不能使用 C++20
- ✓ 追求最小内存占用

### 使用协程方式当：
- ✓ 需要复杂的业务逻辑
- ✓ 需要顺序的异步操作
- ✓ 需要状态管理
- ✓ 重视代码可读性和维护性
- ✓ 可以使用 C++20
- ✓ **推荐用于新项目！**

## 迁移指南

### 从回调迁移到协程

#### 步骤 1：改变订阅方式
```cpp
// 旧
auto sub_id = dispatcher->subscribe([](const Message& msg) { ... });

// 新
auto queue = dispatcher->subscribe();
```

#### 步骤 2：创建协程
```cpp
co_spawn(executor, [queue]() -> awaitable<void> {
    // 在这里处理消息
}, detached);
```

#### 步骤 3：在协程中读取消息
```cpp
co_spawn(executor, [queue]() -> awaitable<void> {
    while (true) {
        auto [ec, msg] = co_await queue->async_read_msg(use_awaitable);
        if (ec) break;
        
        // 原来回调中的处理逻辑放在这里
        process(msg);
    }
}, detached);
```

## 完整示例对比

### 场景：处理消息，每 10 条打印一次统计

#### 回调版本
```cpp
#include "dispatcher.hpp"

int main() {
    asio::io_context io_context;
    auto dispatcher = bcast::make_dispatcher<int>(io_context);
    
    int count = 0;
    int sum = 0;
    
    auto sub = dispatcher->subscribe([&count, &sum](int value) {
        count++;
        sum += value;
        
        if (count % 10 == 0) {
            std::cout << "Processed " << count 
                      << " messages, sum=" << sum << std::endl;
        }
    });
    
    // 发布消息
    for (int i = 1; i <= 100; ++i) {
        dispatcher->publish(i);
    }
    
    io_context.run();
    return 0;
}
```

#### 协程版本
```cpp
#include "dispatcher.hpp"
#include <asio/co_spawn.hpp>
#include <asio/use_awaitable.hpp>

using asio::awaitable;
using asio::use_awaitable;

awaitable<void> processor(auto queue) {
    int count = 0;
    int sum = 0;
    
    while (true) {
        auto [ec, value] = co_await queue->async_read_msg(use_awaitable);
        if (ec) break;
        
        count++;
        sum += value;
        
        if (count % 10 == 0) {
            std::cout << "Processed " << count 
                      << " messages, sum=" << sum << std::endl;
        }
    }
}

int main() {
    asio::io_context io_context;
    auto dispatcher = bcast::make_dispatcher<int>(io_context);
    
    auto queue = dispatcher->subscribe();
    co_spawn(io_context, processor(queue), asio::detached);
    
    // 发布消息
    for (int i = 1; i <= 100; ++i) {
        dispatcher->publish(i);
    }
    
    io_context.run();
    return 0;
}
```

## 总结

新的协程 API：
- ✅ **更直观**：用同步的方式写异步代码
- ✅ **更强大**：支持复杂的控制流和错误处理
- ✅ **更简单**：`publish()` + `co_await read()`
- ✅ **更易维护**：代码逻辑清晰，易于理解

**推荐所有新项目使用协程 API！** 🚀

