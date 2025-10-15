# 异步信号量和队列的取消支持

## 问题背景

之前的实现存在两个问题：

1. **超时无法取消等待**：在 `async_read_msg_with_timeout()` 中，当超时发生时，虽然超时处理器会返回错误，但 `semaphore_.acquire()` 操作仍然在等待队列中。当消息到达时，这个已经超时的等待者会被唤醒并消耗一个信号量槽位，导致资源浪费。

2. **stop() 无法唤醒等待者**：`async_queue_v2::stop()` 方法无法唤醒所有正在等待的读操作，导致它们会一直挂起。

## 解决方案

为 `async_semaphore` 添加了取消支持：

### 1. 新增 API

#### `acquire_cancellable(handler)` 
返回一个 `waiter_id`，可以用来取消这个等待操作。

```cpp
uint64_t waiter_id = sem.acquire_cancellable([&]() {
    // 当信号量可用时调用
    std::cout << "获取成功\n";
});

// 可以随时取消
sem.cancel(waiter_id);
```

#### `cancel(waiter_id)`
取消指定的等待操作。被取消的等待者不会被唤醒，其 handler 不会被调用。

```cpp
sem.cancel(waiter_id);
```

#### `cancel_all()`
取消所有等待操作。用于 `stop()` 场景。

```cpp
sem.cancel_all();
```

### 2. 内部机制

- 每个等待者都有一个唯一的 ID（从 1 开始，0 表示无效）
- `handler_base` 增加了 `cancelled_` 标志和 `cancel()` 方法
- `release()` 在唤醒等待者之前会跳过已取消的等待者
- 被取消的等待者的 handler 不会被调用，只是被标记为取消然后等待清理

### 3. 使用示例

#### 带超时的读取（支持取消）

```cpp
template<typename Duration>
auto async_read_msg_with_timeout(Duration timeout) {
    return asio::async_initiate<CompletionToken, void(std::error_code, T)>(
        [self = this->shared_from_this(), timeout](auto handler) mutable {
            auto timer = std::make_shared<asio::steady_timer>(self->io_context_);
            auto completed = std::make_shared<std::atomic<bool>>(false);
            auto waiter_id = std::make_shared<uint64_t>(0);
            
            // 可取消的 acquire
            *waiter_id = self->semaphore_.acquire_cancellable(
                [self, completed, timer, handler, waiter_id]() mutable {
                    if (completed->exchange(true)) {
                        return;  // 已超时
                    }
                    
                    timer->cancel();
                    // 处理消息...
                }
            );
            
            // 超时处理
            timer->expires_after(timeout);
            timer->async_wait([self, completed, handler, waiter_id](const std::error_code& ec) mutable {
                if (!ec && !completed->exchange(true)) {
                    // 超时：取消 semaphore 等待
                    self->semaphore_.cancel(*waiter_id);
                    handler(std::make_error_code(std::errc::timed_out), T{});
                }
            });
        },
        token
    );
}
```

#### 停止队列

```cpp
void stop() {
    asio::post(strand_, [self = this->shared_from_this()]() {
        self->stopped_ = true;
        self->queue_.clear();
        
        // 取消所有等待者
        self->semaphore_.cancel_all();
    });
}
```

## 关键优化

1. **无资源泄漏**：被取消的等待者不会占用信号量槽位
2. **自动清理**：`release()` 会自动跳过已取消的等待者
3. **线程安全**：所有操作都在 strand 上串行执行
4. **高效**：使用 `uint64_t` ID 和原子操作，开销很小

## 测试

见 `test_cancellation.cpp`，包含以下测试：

1. 超时取消
2. 超时后消息到达（验证不会错误消费）
3. 手动取消
4. 取消所有等待者（stop）

## 向后兼容

- 原有的 `acquire()` API 保持不变
- 新增的 API 是可选的，不影响现有代码
- 只有需要取消功能时才使用 `acquire_cancellable()`

## 性能影响

- `release()` 需要跳过已取消的等待者，在最坏情况下需要遍历队列前面的所有已取消等待者
- 实际上，被取消的等待者很快就会被清理，所以性能影响很小
- ID 生成使用原子操作，开销可忽略

