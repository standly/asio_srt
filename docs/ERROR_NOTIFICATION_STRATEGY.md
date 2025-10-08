# SRT 错误事件通知策略

## 🎯 核心问题

当 `srt_epoll_uwait` 报告 `SRT_EPOLL_ERR` 时，如何通知正在等待的 handler？

## 📊 可能的场景

### 场景分析

```cpp
struct EventOperation {
    std::function<void(std::error_code, int)> read_handler;   // 等待可读
    std::function<void(std::error_code, int)> write_handler;  // 等待可写
    uint32_t events;  // 当前注册的事件：SRT_EPOLL_IN | SRT_EPOLL_OUT | SRT_EPOLL_ERR
};
```

| 场景 | read_handler | write_handler | 如何通知 |
|------|--------------|---------------|----------|
| 1 | ✅ 有 | ❌ 无 | 只通知 read_handler |
| 2 | ❌ 无 | ✅ 有 | 只通知 write_handler |
| 3 | ✅ 有 | ✅ 有 | **都要通知** |
| 4 | ❌ 无 | ❌ 无 | 不应该出现（已清理） |

## 🔍 详细策略

### 策略 A: 通知所有等待的 handler（推荐）✅

**原理**：错误是全局的，影响整个 socket，所有等待者都应该知道。

```cpp
// 在 poll_loop 中处理错误
if (flags & SRT_EPOLL_ERR) {
    auto ec = asrt::make_srt_error_code();
    
    // 通知所有正在等待的 handler
    std::vector<std::function<void(std::error_code, int)>> handlers_to_call;
    
    if (op->read_handler) {
        handlers_to_call.push_back(std::move(op->read_handler));
        op->clear_handler(SRT_EPOLL_IN);
    }
    
    if (op->write_handler) {
        handlers_to_call.push_back(std::move(op->write_handler));
        op->clear_handler(SRT_EPOLL_OUT);
    }
    
    // 调用所有 handler
    for (auto& h : handlers_to_call) {
        auto ex = asio::get_associated_executor(h, io_context_.get_executor());
        asio::post(ex, [h = std::move(h), ec, flags]() mutable {
            h(ec, flags);  // 传递错误码
        });
    }
    
    // 从 epoll 中移除并清理
    srt_epoll_remove_usock(srt_epoll_id_, sock);
    pending_ops_.erase(it);
    
    return;  // 错误处理完成，不再处理其他事件
}
```

**优点**：
- ✅ 语义清晰：错误影响整个 socket
- ✅ 及时通知：所有等待者都能立即知道
- ✅ 资源释放：一次性清理所有 handler

**缺点**：
- ⚠️ 可能重复通知：同一个错误被两个 handler 接收

**用户代码示例**：
```cpp
// 协程 1: 等待可读
asio::co_spawn(ctx, [&]() -> asio::awaitable<void> {
    try {
        co_await reactor.async_wait_readable(sock);
    } catch (const asio::system_error& e) {
        std::cerr << "Read waiter got error: " << e.what() << "\n";
        // ← 会收到错误通知
    }
}, asio::detached);

// 协程 2: 等待可写
asio::co_spawn(ctx, [&]() -> asio::awaitable<void> {
    try {
        co_await reactor.async_wait_writable(sock);
    } catch (const asio::system_error& e) {
        std::cerr << "Write waiter got error: " << e.what() << "\n";
        // ← 也会收到错误通知
    }
}, asio::detached);

// 如果 socket 出错，两个协程都会捕获到异常
```

---

### 策略 B: 根据事件标志选择性通知

**原理**：SRT 可能同时返回 `SRT_EPOLL_ERR | SRT_EPOLL_IN` 或 `SRT_EPOLL_ERR | SRT_EPOLL_OUT`。

```cpp
if (flags & SRT_EPOLL_ERR) {
    auto ec = asrt::make_srt_error_code();
    
    // 根据事件标志决定通知谁
    if ((flags & SRT_EPOLL_IN) && op->read_handler) {
        // 只通知 read_handler
        auto h = std::move(op->read_handler);
        asio::post(io_context_, [h = std::move(h), ec, flags]() mutable {
            h(ec, flags);
        });
        op->clear_handler(SRT_EPOLL_IN);
    }
    
    if ((flags & SRT_EPOLL_OUT) && op->write_handler) {
        // 只通知 write_handler
        auto h = std::move(op->write_handler);
        asio::post(io_context_, [h = std::move(h), ec, flags]() mutable {
            h(ec, flags);
        });
        op->clear_handler(SRT_EPOLL_OUT);
    }
    
    // 如果两者都没有，但有其他 handler，也要通知
    if (!op->is_empty()) {
        // 还有其他 handler，通知它
        if (op->read_handler) {
            auto h = std::move(op->read_handler);
            asio::post(io_context_, [h = std::move(h), ec]() mutable {
                h(ec, SRT_EPOLL_ERR);
            });
            op->clear_handler(SRT_EPOLL_IN);
        }
        if (op->write_handler) {
            auto h = std::move(op->write_handler);
            asio::post(io_context_, [h = std::move(h), ec]() mutable {
                h(ec, SRT_EPOLL_ERR);
            });
            op->clear_handler(SRT_EPOLL_OUT);
        }
    }
    
    // 清理
    srt_epoll_remove_usock(srt_epoll_id_, sock);
    pending_ops_.erase(it);
    return;
}
```

**优点**：
- ✅ 更精确：只通知相关的 handler

**缺点**：
- ❌ 复杂：逻辑复杂，容易出错
- ❌ 不完整：如果 SRT 只返回 `SRT_EPOLL_ERR`（没有 IN/OUT），仍需通知所有

---

### 策略 C: 分阶段处理（不推荐）

**原理**：先处理错误，再处理正常事件。

```cpp
// 第一阶段：处理错误
if (flags & SRT_EPOLL_ERR) {
    auto ec = asrt::make_srt_error_code();
    // 记录错误但不立即通知
}

// 第二阶段：处理正常事件，传递错误
if (flags & SRT_EPOLL_IN) {
    h(ec, flags);  // 如果有错误，ec 不为空
}
if (flags & SRT_EPOLL_OUT) {
    h(ec, flags);  // 如果有错误，ec 不为空
}
```

**缺点**：
- ❌ 混乱：正常事件和错误混合
- ❌ 不直观：用户难以理解

---

## 🎯 推荐方案：策略 A（通知所有等待者）

### 完整实现

```cpp
void SrtReactor::poll_loop() {
    std::vector<SRT_EPOLL_EVENT> events(100);
    
    while (running_) {
        // ... 检查 pending_ops 的逻辑 ...
        
        int n = srt_epoll_uwait(srt_epoll_id_, events.data(), events.size(), 100);
        
        if (n <= 0) continue;
        
        for (int i = 0; i < n; i++) {
            SRTSOCKET sock = events[i].fd;
            int flags = events[i].events;
            
            asio::post(op_strand_, [this, sock, flags]() {
                auto it = pending_ops_.find(sock);
                if (it == pending_ops_.end()) {
                    return; // 已被取消
                }
                
                auto& op = it->second;
                
                // ========================================
                // 优先处理错误 - 通知所有等待者
                // ========================================
                if (flags & SRT_EPOLL_ERR) {
                    auto ec = asrt::make_srt_error_code();
                    
                    // 收集所有需要通知的 handler
                    std::vector<std::pair<
                        std::function<void(std::error_code, int)>,
                        asio::any_io_executor
                    >> handlers_to_call;
                    
                    if (op->read_handler) {
                        auto ex = asio::get_associated_executor(
                            op->read_handler, 
                            io_context_.get_executor()
                        );
                        handlers_to_call.emplace_back(
                            std::move(op->read_handler), 
                            ex
                        );
                        op->clear_handler(SRT_EPOLL_IN);
                    }
                    
                    if (op->write_handler) {
                        auto ex = asio::get_associated_executor(
                            op->write_handler, 
                            io_context_.get_executor()
                        );
                        handlers_to_call.emplace_back(
                            std::move(op->write_handler), 
                            ex
                        );
                        op->clear_handler(SRT_EPOLL_OUT);
                    }
                    
                    // 从 epoll 中移除
                    srt_epoll_remove_usock(srt_epoll_id_, sock);
                    pending_ops_.erase(it);
                    
                    // 通知所有 handler（在 strand 外执行）
                    for (auto& [handler, executor] : handlers_to_call) {
                        asio::post(executor, [h = std::move(handler), ec, flags]() mutable {
                            h(ec, flags);
                        });
                    }
                    
                    return; // 错误处理完成
                }
                
                // ========================================
                // 处理正常的可读/可写事件
                // ========================================
                
                std::vector<std::pair<
                    std::function<void(std::error_code, int)>,
                    asio::any_io_executor
                >> handlers_to_call;
                
                if ((flags & SRT_EPOLL_IN) && op->read_handler) {
                    auto ex = asio::get_associated_executor(
                        op->read_handler, 
                        io_context_.get_executor()
                    );
                    handlers_to_call.emplace_back(
                        std::move(op->read_handler), 
                        ex
                    );
                    op->clear_handler(SRT_EPOLL_IN);
                }
                
                if ((flags & SRT_EPOLL_OUT) && op->write_handler) {
                    auto ex = asio::get_associated_executor(
                        op->write_handler, 
                        io_context_.get_executor()
                    );
                    handlers_to_call.emplace_back(
                        std::move(op->write_handler), 
                        ex
                    );
                    op->clear_handler(SRT_EPOLL_OUT);
                }
                
                // 清理或更新
                if (op->is_empty()) {
                    srt_epoll_remove_usock(srt_epoll_id_, sock);
                    pending_ops_.erase(it);
                } else {
                    int srt_events = op->events;
                    srt_epoll_update_usock(srt_epoll_id_, sock, &srt_events);
                }
                
                // 通知所有 handler（成功，无错误）
                for (auto& [handler, executor] : handlers_to_call) {
                    asio::post(executor, [h = std::move(handler), flags]() mutable {
                        h({}, flags);  // 空 error_code = 成功
                    });
                }
            });
        }
    }
}
```

### 关键要点

1. **错误优先处理**
   ```cpp
   if (flags & SRT_EPOLL_ERR) {
       // 立即处理，不再检查其他事件
       return;
   }
   ```

2. **通知所有等待者**
   ```cpp
   // 读等待者和写等待者都会收到错误通知
   if (op->read_handler) { notify(ec); }
   if (op->write_handler) { notify(ec); }
   ```

3. **一次性清理**
   ```cpp
   // 从 epoll 移除
   srt_epoll_remove_usock(srt_epoll_id_, sock);
   // 从 pending_ops 删除
   pending_ops_.erase(it);
   ```

---

## 📝 用户体验

### 示例 1: 只等待可读

```cpp
asio::co_spawn(ctx, [&]() -> asio::awaitable<void> {
    try {
        // 只注册了 read_handler
        co_await reactor.async_wait_readable(sock);
        
        // 成功，可以读取
        char buf[1500];
        int n = srt_recv(sock, buf, sizeof(buf));
        
    } catch (const asio::system_error& e) {
        // Socket 错误会在这里捕获
        std::cerr << "Error: " << e.what() << "\n";
    }
}, asio::detached);
```

**错误发生时**：
- `SRT_EPOLL_ERR` → read_handler 被调用并传递错误码
- 协程抛出 `asio::system_error`
- ✅ 用户立即知道出错了

### 示例 2: 同时等待可读和可写

```cpp
// 协程 1
asio::co_spawn(ctx, [&]() -> asio::awaitable<void> {
    try {
        co_await reactor.async_wait_readable(sock);
        // 读操作
    } catch (const asio::system_error& e) {
        std::cerr << "Read error: " << e.what() << "\n";
    }
}, asio::detached);

// 协程 2
asio::co_spawn(ctx, [&]() -> asio::awaitable<void> {
    try {
        co_await reactor.async_wait_writable(sock);
        // 写操作
    } catch (const asio::system_error& e) {
        std::cerr << "Write error: " << e.what() << "\n";
    }
}, asio::detached);
```

**错误发生时**：
- `SRT_EPOLL_ERR` → 两个 handler 都被调用
- 两个协程都会捕获异常
- ✅ 所有等待者都被通知

### 示例 3: 取消操作

```cpp
asio::cancellation_signal cancel_signal;

asio::co_spawn(ctx, [&]() -> asio::awaitable<void> {
    try {
        co_await reactor.async_wait_readable(
            sock,
            asio::bind_cancellation_slot(
                cancel_signal.slot(),
                asio::use_awaitable
            )
        );
    } catch (const asio::system_error& e) {
        if (e.code() == asio::error::operation_aborted) {
            std::cerr << "Cancelled\n";
        } else {
            std::cerr << "Socket error: " << e.what() << "\n";
        }
    }
}, asio::detached);

// 稍后取消
cancel_signal.emit(asio::cancellation_type::all);
```

---

## 🔍 边界情况

### 情况 1: SRT 只返回 ERR（无 IN/OUT）

```cpp
events[0] = {sock, SRT_EPOLL_ERR};  // 只有 ERR

// 我们的处理：通知所有等待者
if (op->read_handler) { notify_error(); }
if (op->write_handler) { notify_error(); }
```

### 情况 2: SRT 返回 ERR + IN

```cpp
events[0] = {sock, SRT_EPOLL_ERR | SRT_EPOLL_IN};

// 我们的处理：优先处理错误，忽略 IN
if (flags & SRT_EPOLL_ERR) {
    notify_all_error();
    return;  // 不再处理 IN
}
```

**理由**：如果有错误，"可读" 没有意义（读取会失败）。

### 情况 3: 错误后立即取消其他操作

```cpp
// Socket 错误
SRT_EPOLL_ERR → 
    ↓
清理 read_handler ✅
清理 write_handler ✅
从 epoll 移除 ✅
从 pending_ops 删除 ✅
    ↓
socket 不再被监控 ✅
```

---

## ✅ 测试用例

需要添加的测试：

```cpp
// 测试 1: 错误时通知读等待者
TEST_F(SrtReactorTest, ErrorNotifiesReadWaiter);

// 测试 2: 错误时通知写等待者
TEST_F(SrtReactorTest, ErrorNotifiesWriteWaiter);

// 测试 3: 错误时通知所有等待者
TEST_F(SrtReactorTest, ErrorNotifiesAllWaiters);

// 测试 4: 错误后清理资源
TEST_F(SrtReactorTest, ErrorCleansUpResources);
```

---

## 总结

**推荐策略**：当检测到 `SRT_EPOLL_ERR` 时，**通知所有正在等待的 handler**。

**理由**：
1. ✅ 语义清晰：错误影响整个 socket
2. ✅ 及时通知：所有等待者都能立即知道
3. ✅ 简单可靠：实现逻辑清晰，不易出错
4. ✅ 资源高效：一次性清理所有相关资源
5. ✅ 用户友好：符合直觉，易于理解

这种策略确保了当 socket 出现任何错误时，所有相关的异步操作都能及时得到通知并正确处理。


