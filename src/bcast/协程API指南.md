# 协程 API 使用指南

## 简介

新的协程 API 让发布-订阅模式的使用变得极其简单！

**核心理念：**
- 📤 发布者：使用 `dispatcher->publish(msg)` 发送消息
- 📥 订阅者：使用 `co_await queue->async_read_msg()` 读取消息

## 为什么使用协程？

### 传统回调方式
```cpp
auto sub_id = dispatcher->subscribe([](const Message& msg) {
    // 处理消息
    process(msg);
    
    // 难以添加复杂逻辑
    if (condition) {
        // 回调嵌套...
        another_async_call([](Result res) {
            // 更多嵌套...
        });
    }
});
```

### 协程方式
```cpp
auto queue = dispatcher->subscribe();

co_spawn(executor, [queue]() -> awaitable<void> {
    while (true) {
        // 像同步代码一样写异步逻辑！
        auto [ec, msg] = co_await queue->async_read_msg(use_awaitable);
        if (ec) break;
        
        // 轻松添加控制流
        process(msg);
        
        if (condition) {
            auto result = co_await another_async_call(use_awaitable);
            // 继续处理...
        }
    }
}, detached);
```

## 快速开始

### 1. 最简单的例子

```cpp
#include "dispatcher.hpp"
#include <asio.hpp>
#include <asio/co_spawn.hpp>
#include <asio/use_awaitable.hpp>

using asio::awaitable;
using asio::use_awaitable;

awaitable<void> subscriber_task(std::shared_ptr<bcast::async_queue<std::string>> queue) {
    while (true) {
        // 读取消息 - 就这么简单！
        auto [ec, msg] = co_await queue->async_read_msg(use_awaitable);
        
        if (ec) {
            std::cout << "Queue stopped" << std::endl;
            break;
        }
        
        std::cout << "Received: " << msg << std::endl;
    }
}

int main() {
    asio::io_context io_context;
    
    // 创建 dispatcher
    auto dispatcher = bcast::make_dispatcher<std::string>(io_context);
    
    // 订阅 - 获取队列
    auto queue = dispatcher->subscribe();
    
    // 启动订阅者协程
    co_spawn(io_context, subscriber_task(queue), asio::detached);
    
    // 发布消息
    dispatcher->publish("Hello, Coroutines!");
    dispatcher->publish("这太简单了！");
    
    // 运行
    io_context.run();
}
```

### 2. 批量读取消息

```cpp
awaitable<void> batch_processor(std::shared_ptr<bcast::async_queue<int>> queue) {
    while (true) {
        // 一次读取最多 10 条消息
        auto [ec, messages] = co_await queue->async_read_msgs(10, use_awaitable);
        
        if (ec) break;
        
        // 批量处理
        std::cout << "Processing batch of " << messages.size() << " messages" << std::endl;
        for (int msg : messages) {
            process(msg);
        }
    }
}
```

## API 参考

### dispatcher<T>

#### 订阅
```cpp
std::shared_ptr<async_queue<T>> subscribe()
```
返回一个队列，用于读取消息。

**示例：**
```cpp
auto queue = dispatcher->subscribe();
```

#### 发布
```cpp
void publish(const T& msg)  // 复制版本
void publish(T&& msg)       // 移动版本
```
向所有订阅者发布消息。

**示例：**
```cpp
dispatcher->publish(Message{"test", 42});  // 复制
dispatcher->publish(std::move(large_msg)); // 移动
```

#### 取消订阅
```cpp
void unsubscribe(std::shared_ptr<async_queue<T>> queue)
```

**示例：**
```cpp
dispatcher->unsubscribe(queue);
```

### async_queue<T>

#### 读取单条消息
```cpp
auto async_read_msg(CompletionToken token)
```

返回：`std::pair<std::error_code, T>`

**示例：**
```cpp
// 协程方式
auto [ec, msg] = co_await queue->async_read_msg(use_awaitable);

// 回调方式
queue->async_read_msg([](std::error_code ec, Message msg) {
    if (!ec) {
        process(msg);
    }
});
```

#### 读取多条消息
```cpp
auto async_read_msgs(size_t max_count, CompletionToken token)
```

返回：`std::pair<std::error_code, std::vector<T>>`

**示例：**
```cpp
auto [ec, messages] = co_await queue->async_read_msgs(100, use_awaitable);
if (!ec) {
    for (const auto& msg : messages) {
        process(msg);
    }
}
```

#### 停止队列
```cpp
void stop()
```

#### 检查状态
```cpp
bool is_stopped() const
size_t size() const  // 当前队列大小
```

## 实用模式

### 模式 1: 循环处理消息

```cpp
awaitable<void> message_loop(auto queue) {
    try {
        while (!queue->is_stopped()) {
            auto [ec, msg] = co_await queue->async_read_msg(use_awaitable);
            
            if (ec) {
                if (ec == asio::error::operation_aborted) {
                    std::cout << "Queue stopped" << std::endl;
                } else {
                    std::cout << "Error: " << ec.message() << std::endl;
                }
                break;
            }
            
            process(msg);
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
}
```

### 模式 2: 超时读取

```cpp
awaitable<void> read_with_timeout(auto queue) {
    using namespace asio::experimental::awaitable_operators;
    
    auto executor = co_await asio::this_coro::executor;
    asio::steady_timer timer(executor);
    timer.expires_after(std::chrono::seconds(5));
    
    try {
        // 等待消息或超时
        auto result = co_await (
            queue->async_read_msg(use_awaitable) ||
            timer.async_wait(use_awaitable)
        );
        
        if (result.index() == 0) {
            // 收到消息
            auto [ec, msg] = std::get<0>(result);
            process(msg);
        } else {
            // 超时
            std::cout << "Timeout!" << std::endl;
        }
    } catch (...) {
        // Handle exception
    }
}
```

### 模式 3: 有选择地处理消息

```cpp
awaitable<void> selective_processor(auto queue) {
    while (true) {
        auto [ec, msg] = co_await queue->async_read_msg(use_awaitable);
        if (ec) break;
        
        // 只处理高优先级消息
        if (msg.priority >= 5) {
            process_important(msg);
        } else {
            // 低优先级消息可以忽略或延迟处理
            schedule_later(msg);
        }
    }
}
```

### 模式 4: 多队列并发读取

```cpp
awaitable<void> multi_queue_reader(auto queue1, auto queue2) {
    using namespace asio::experimental::awaitable_operators;
    
    while (true) {
        // 同时等待两个队列
        auto result = co_await (
            queue1->async_read_msg(use_awaitable) ||
            queue2->async_read_msg(use_awaitable)
        );
        
        if (result.index() == 0) {
            auto [ec, msg] = std::get<0>(result);
            process_queue1(msg);
        } else {
            auto [ec, msg] = std::get<1>(result);
            process_queue2(msg);
        }
    }
}
```

### 模式 5: 批量处理优化

```cpp
awaitable<void> batch_optimizer(auto queue) {
    std::vector<Message> batch;
    batch.reserve(100);
    
    auto executor = co_await asio::this_coro::executor;
    asio::steady_timer timer(executor);
    
    while (true) {
        // 设置批量超时
        timer.expires_after(std::chrono::milliseconds(100));
        
        // 收集消息直到达到批量大小或超时
        while (batch.size() < 100) {
            using namespace asio::experimental::awaitable_operators;
            
            auto result = co_await (
                queue->async_read_msg(use_awaitable) ||
                timer.async_wait(use_awaitable)
            );
            
            if (result.index() == 0) {
                auto [ec, msg] = std::get<0>(result);
                if (ec) break;
                batch.push_back(std::move(msg));
            } else {
                // 超时，处理当前批次
                break;
            }
        }
        
        if (!batch.empty()) {
            process_batch(batch);
            batch.clear();
        }
    }
}
```

## 实战场景

### 场景 1: WebSocket 消息广播

```cpp
class WebSocketSession {
public:
    awaitable<void> start(auto dispatcher) {
        // 订阅消息
        auto queue = dispatcher->subscribe();
        
        // 启动读取协程
        co_await receive_and_broadcast(queue);
    }
    
private:
    awaitable<void> receive_and_broadcast(auto queue) {
        while (ws_.is_open()) {
            // 从队列读取要广播的消息
            auto [ec, msg] = co_await queue->async_read_msg(use_awaitable);
            if (ec) break;
            
            // 通过 WebSocket 发送
            co_await ws_.async_write(asio::buffer(msg), use_awaitable);
        }
    }
    
    websocket::stream<tcp::socket> ws_;
};
```

### 场景 2: 实时日志聚合

```cpp
class LogAggregator {
public:
    awaitable<void> run(auto log_queue) {
        std::ofstream log_file("app.log");
        
        while (true) {
            // 批量读取日志
            auto [ec, logs] = co_await log_queue->async_read_msgs(100, use_awaitable);
            if (ec) break;
            
            // 批量写入文件
            for (const auto& log : logs) {
                log_file << format_log(log) << std::endl;
            }
            log_file.flush();
        }
    }
};
```

### 场景 3: 事件驱动状态机

```cpp
class StateMachine {
public:
    awaitable<void> run(auto event_queue) {
        State state = State::IDLE;
        
        while (true) {
            auto [ec, event] = co_await event_queue->async_read_msg(use_awaitable);
            if (ec) break;
            
            // 状态转换
            switch (state) {
                case State::IDLE:
                    if (event.type == EventType::START) {
                        state = State::RUNNING;
                        on_start();
                    }
                    break;
                    
                case State::RUNNING:
                    if (event.type == EventType::STOP) {
                        state = State::IDLE;
                        on_stop();
                    } else if (event.type == EventType::PROCESS) {
                        co_await process_event(event);
                    }
                    break;
            }
        }
    }
    
private:
    enum class State { IDLE, RUNNING };
};
```

## 性能考虑

### 1. 消息复制优化

```cpp
// ❌ 大消息会被复制多次
struct LargeMessage {
    std::vector<uint8_t> data;  // 可能很大
};
dispatcher->publish(large_msg);  // 每个订阅者都复制

// ✅ 使用 shared_ptr
using MessagePtr = std::shared_ptr<LargeMessage>;
auto dispatcher = make_dispatcher<MessagePtr>(io_context);
dispatcher->publish(std::make_shared<LargeMessage>(...));  // 只复制指针
```

### 2. 批量处理

```cpp
// ✅ 批量读取减少系统调用
auto [ec, messages] = co_await queue->async_read_msgs(100, use_awaitable);
process_batch(messages);  // 一次处理多条
```

### 3. 避免队列积压

```cpp
awaitable<void> monitor_queue(auto queue) {
    asio::steady_timer timer(co_await asio::this_coro::executor);
    
    while (true) {
        timer.expires_after(std::chrono::seconds(1));
        co_await timer.async_wait(use_awaitable);
        
        if (queue->size() > 10000) {
            std::cerr << "Warning: Queue backlog " << queue->size() << std::endl;
            // 采取措施：停止接收、增加处理速度等
        }
    }
}
```

## 错误处理

### 优雅关闭

```cpp
awaitable<void> graceful_shutdown(auto queue, auto dispatcher) {
    try {
        while (true) {
            auto [ec, msg] = co_await queue->async_read_msg(use_awaitable);
            
            if (ec == asio::error::operation_aborted) {
                // 队列被停止
                std::cout << "Shutting down gracefully..." << std::endl;
                break;
            } else if (ec) {
                // 其他错误
                std::cerr << "Error: " << ec.message() << std::endl;
                break;
            }
            
            process(msg);
        }
        
        // 清理资源
        cleanup();
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
    }
}
```

## 编译要求

- **C++20** 或更高版本
- 支持协程的编译器：
  - GCC 10+
  - Clang 10+
  - MSVC 2019 16.8+

### 编译命令

```bash
# GCC
g++ -std=c++20 -fcoroutines your_code.cpp -lpthread -o app

# Clang
clang++ -std=c++20 -stdlib=libc++ your_code.cpp -lpthread -o app

# CMake
set(CMAKE_CXX_STANDARD 20)
```

## 总结

新的协程 API 提供了：

| 特性 | 说明 |
|------|------|
| 🎯 **简单** | `publish()` 发送，`co_await read()` 接收 |
| 🔄 **直观** | 用同步风格写异步代码 |
| 🚀 **高效** | 无锁设计，零开销抽象 |
| 💪 **强大** | 支持超时、并发、批量处理 |
| 🛡️ **安全** | 类型安全，异常友好 |

**开始使用：**
```cpp
auto queue = dispatcher->subscribe();
auto [ec, msg] = co_await queue->async_read_msg(use_awaitable);
```

就是这么简单！🎉

