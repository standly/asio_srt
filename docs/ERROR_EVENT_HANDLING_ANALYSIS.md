# SRT 错误事件处理方式对比分析

## 🔍 当前问题

我们在注册事件时包含了 `SRT_EPOLL_ERR`：
```cpp
async_add_op(srt_sock, SRT_EPOLL_IN | SRT_EPOLL_ERR, ...);
async_add_op(srt_sock, SRT_EPOLL_OUT | SRT_EPOLL_ERR, ...);
```

但在 `poll_loop` 中使用 `srt_epoll_wait`，只处理了 read 和 write 事件：
```cpp
srt_epoll_wait(srt_epoll_id_, 
               read_fds.data(), &read_num,   // 只获取可读的 socket
               write_fds.data(), &write_num, // 只获取可写的 socket
               100, 
               nullptr, nullptr,              // 系统 socket (不使用)
               nullptr, nullptr);
```

## 📊 两种处理方式对比

### 方式 1: 错误信息合并到 read/write 中（当前方式）

#### API: `srt_epoll_wait`

```cpp
int srt_epoll_wait(
    int eid,
    SRTSOCKET* readfds,  int* rnum,     // 可读的 socket 列表
    SRTSOCKET* writefds, int* wnum,     // 可写的 socket 列表
    int64_t msTimeOut,
    SYSSOCKET* lrfds,    int* lrnum,    // 系统 socket (通常不用)
    SYSSOCKET* lwfds,    int* lwnum
);
```

#### 行为机制

**SRT 的内部行为**：
1. 当 socket 发生错误时，它会**同时出现在 read 和 write 列表中**
2. 用户需要在读/写操作时检查 SRT 错误状态
3. 错误信息通过 `srt_getlasterror()` 获取

**示例场景**：
```cpp
// Socket 发生连接丢失
srt_epoll_wait(...) 返回：
  - read_fds:  [socket_123]  // ✅ 包含该 socket
  - write_fds: [socket_123]  // ✅ 也包含该 socket

// 用户代码：
if (有读事件) {
    int n = srt_recv(sock, buffer, size);
    if (n < 0) {
        // 现在才发现是错误
        int err = srt_getlasterror(nullptr);
    }
}
```

#### ✅ 优点

1. **简单直观**
   - API 简单，只需处理两个列表
   - 不需要检查事件标志位

2. **兼容性好**
   - 类似 POSIX `select/poll` 的接口风格
   - 易于从传统网络编程迁移

3. **代码量少**
   - 处理逻辑简单
   - 当前实现已经工作正常

4. **性能开销小**
   - 不需要解析事件掩码
   - 直接处理 socket 列表

#### ❌ 缺点

1. **不能区分真正的可读/可写和错误**
   ```cpp
   // Socket 同时出现在 read_fds 和 write_fds
   // 无法知道是：
   // - 真的可读可写？
   // - 还是发生了错误？
   ```

2. **可能延迟发现错误**
   ```cpp
   // 场景：socket 已断开连接
   process_fds(read_fds, read_num, SRT_EPOLL_IN);
   // ↑ 触发 read_handler
   // ↓ 用户在 handler 中才发现错误
   
   co_await reactor.async_wait_readable(sock);
   // 返回成功
   
   int n = srt_recv(sock, buffer, size);
   // ← 这里才返回错误！
   ```

3. **资源浪费**
   - 错误的 socket 会触发读写 handler
   - 用户需要额外调用 `srt_recv/srt_send` 才能发现错误
   - 增加了不必要的上下文切换

4. **错误处理不优雅**
   - 用户期望 `async_wait_readable` 返回后可以立即读取
   - 但实际可能是错误状态

5. **与 ASIO 其他协议不一致**
   ```cpp
   // ASIO TCP 的行为：
   co_await socket.async_wait(tcp::socket::wait_read);
   // 如果底层发生错误，会直接抛出异常
   
   // 当前 SRT 的行为：
   co_await reactor.async_wait_readable(sock);
   // 返回成功，但 socket 可能已经出错
   ```

#### 🔧 当前实现的问题

```cpp
// srt_reactor.cpp - poll_loop()
process_fds(read_fds, read_num, SRT_EPOLL_IN);
process_fds(write_fds, write_num, SRT_EPOLL_OUT);

// process_fds() 简单地调用 handler
h({}, event_type);  // ← 总是传递空 error_code！
```

**问题**：即使 socket 出错了，我们也无法知道！

---

### 方式 2: 错误信息单独处理（推荐方式）

#### API: `srt_epoll_uwait`

```cpp
typedef struct SRT_EPOLL_EVENT_STR {
    SRTSOCKET fd;
    int events;  // SRT_EPOLL_IN | SRT_EPOLL_OUT | SRT_EPOLL_ERR
} SRT_EPOLL_EVENT;

int srt_epoll_uwait(
    int eid, 
    SRT_EPOLL_EVENT* fdsSet,  // 事件数组
    int fdsSize,              // 数组大小
    int64_t msTimeOut
);
```

#### 行为机制

**精确的事件通知**：
```cpp
SRT_EPOLL_EVENT events[100];
int n = srt_epoll_uwait(eid, events, 100, timeout);

for (int i = 0; i < n; i++) {
    SRTSOCKET sock = events[i].fd;
    int flags = events[i].events;
    
    if (flags & SRT_EPOLL_ERR) {
        // 明确知道发生了错误
    }
    if (flags & SRT_EPOLL_IN) {
        // 真正可读
    }
    if (flags & SRT_EPOLL_OUT) {
        // 真正可写
    }
}
```

#### ✅ 优点

1. **精确的事件通知**
   ```cpp
   // 可以明确区分：
   - SRT_EPOLL_IN:  真正可读
   - SRT_EPOLL_OUT: 真正可写
   - SRT_EPOLL_ERR: 发生错误
   - SRT_EPOLL_IN | SRT_EPOLL_ERR: 可读但有错误
   ```

2. **立即发现错误**
   ```cpp
   if (flags & SRT_EPOLL_ERR) {
       // 在 reactor 层面就能发现并处理错误
       auto ec = asrt::make_srt_error_code();
       handler(ec, flags);  // 直接传递错误
       return;
   }
   ```

3. **避免无效操作**
   - 发现错误后直接调用 handler 并传递错误码
   - 不需要用户再调用 `srt_recv/srt_send`
   - 节省系统调用和上下文切换

4. **更符合 ASIO 语义**
   ```cpp
   // 与 ASIO 其他协议一致：
   // 如果底层出错，异步操作会立即完成并报告错误
   
   try {
       co_await reactor.async_wait_readable(sock);
   } catch (const asio::system_error& e) {
       // 如果连接断开等，这里就能捕获
   }
   ```

5. **更好的错误上下文**
   ```cpp
   // 可以记录更详细的信息
   if (flags & SRT_EPOLL_ERR) {
       // 获取具体错误
       auto ec = asrt::make_srt_error_code();
       
       // 可以区分是读等待还是写等待时出错
       if (op->read_handler) {
           op->read_handler(ec, flags);
       }
       if (op->write_handler) {
           op->write_handler(ec, flags);
       }
   }
   ```

6. **更完整的事件处理**
   - 支持多个事件同时触发
   - 例如：`SRT_EPOLL_IN | SRT_EPOLL_OUT` (同时可读可写)

#### ❌ 缺点

1. **代码复杂度略高**
   - 需要检查事件标志位
   - 需要处理多个事件组合

2. **数据结构变化**
   ```cpp
   // 之前：两个简单数组
   std::vector<SRTSOCKET> read_fds;
   std::vector<SRTSOCKET> write_fds;
   
   // 现在：一个事件数组
   std::vector<SRT_EPOLL_EVENT> events;
   ```

3. **需要重构现有代码**
   - `process_fds` 函数需要改写
   - 测试代码可能需要更新

#### 🎯 实现示例

```cpp
void SrtReactor::poll_loop() {
    std::vector<SRT_EPOLL_EVENT> events(100);
    
    while (running_) {
        // ... (检查 pending_ops 的逻辑不变)
        
        // 使用 srt_epoll_uwait
        int n = srt_epoll_uwait(srt_epoll_id_, events.data(), events.size(), 100);
        
        if (n <= 0) continue;
        
        for (int i = 0; i < n; i++) {
            SRTSOCKET sock = events[i].fd;
            int flags = events[i].events;
            
            asio::post(op_strand_, [this, sock, flags]() {
                auto it = pending_ops_.find(sock);
                if (it == pending_ops_.end()) return;
                
                auto& op = it->second;
                
                // 优先处理错误
                if (flags & SRT_EPOLL_ERR) {
                    auto ec = asrt::make_srt_error_code();
                    
                    // 通知所有等待的 handler
                    if ((flags & SRT_EPOLL_IN) && op->read_handler) {
                        auto h = std::move(op->read_handler);
                        asio::post(io_context_, [h = std::move(h), ec, flags]() {
                            h(ec, flags);
                        });
                        op->clear_handler(SRT_EPOLL_IN);
                    }
                    
                    if ((flags & SRT_EPOLL_OUT) && op->write_handler) {
                        auto h = std::move(op->write_handler);
                        asio::post(io_context_, [h = std::move(h), ec, flags]() {
                            h(ec, flags);
                        });
                        op->clear_handler(SRT_EPOLL_OUT);
                    }
                    
                } else {
                    // 正常的可读/可写事件
                    if ((flags & SRT_EPOLL_IN) && op->read_handler) {
                        auto h = std::move(op->read_handler);
                        asio::post(io_context_, [h = std::move(h), flags]() {
                            h({}, flags);  // 无错误
                        });
                        op->clear_handler(SRT_EPOLL_IN);
                    }
                    
                    if ((flags & SRT_EPOLL_OUT) && op->write_handler) {
                        auto h = std::move(op->write_handler);
                        asio::post(io_context_, [h = std::move(h), flags]() {
                            h({}, flags);  // 无错误
                        });
                        op->clear_handler(SRT_EPOLL_OUT);
                    }
                }
                
                // 清理
                if (op->is_empty()) {
                    srt_epoll_remove_usock(srt_epoll_id_, sock);
                    pending_ops_.erase(it);
                } else {
                    int srt_events = op->events;
                    srt_epoll_update_usock(srt_epoll_id_, sock, &srt_events);
                }
            });
        }
    }
}
```

---

## 🎯 决策建议

### 推荐：方式 2 (srt_epoll_uwait)

**理由**：

1. **正确性优先**
   - 能够立即发现并报告错误
   - 避免用户在读写时才发现问题
   - 与 ASIO 其他协议行为一致

2. **更好的用户体验**
   ```cpp
   // 用户期望：
   try {
       co_await reactor.async_wait_readable(sock);
       // 如果成功，socket 应该真的可读
       int n = srt_recv(sock, buffer, size);
       // 这里不应该再遇到连接错误
   } catch (...) {
       // 错误应该在这里被捕获
   }
   ```

3. **性能优势**
   - 错误时避免无效的 handler 调用
   - 减少不必要的系统调用
   - 更早地释放资源

4. **代码质量**
   - 更明确的错误处理逻辑
   - 更容易调试和维护
   - 更符合现代 C++ 异步编程惯例

### 迁移成本

| 方面 | 成本 | 说明 |
|------|------|------|
| 代码修改 | 中 | 主要是 `poll_loop` 函数 |
| 测试更新 | 低 | 现有测试应该仍然通过 |
| 文档更新 | 低 | 行为变化对用户透明 |
| 性能影响 | 正面 | 略有提升 |

### 如果选择方式 1（保持现状）

需要做的改进：

1. **文档说明**
   ```markdown
   ## 注意事项
   
   `async_wait_readable/writable` 只表示事件就绪，不保证操作一定成功。
   用户需要检查 `srt_recv/srt_send` 的返回值。
   ```

2. **可能的改进**
   ```cpp
   // 在 handler 调用前检查 socket 状态
   SRT_SOCKSTATUS status = srt_getsockstate(sock);
   if (status == SRTS_BROKEN || status == SRTS_CLOSED) {
       auto ec = asrt::make_srt_error_code();
       h(ec, flags);
       return;
   }
   ```

   但这仍然不完美，因为：
   - 增加了额外的系统调用
   - 状态检查有竞争条件
   - 不如直接使用事件标志精确

---

## 📋 对比总结表

| 特性 | srt_epoll_wait (方式1) | srt_epoll_uwait (方式2) |
|------|----------------------|------------------------|
| **API 复杂度** | ⭐⭐⭐⭐⭐ 简单 | ⭐⭐⭐⭐ 中等 |
| **错误检测** | ⭐⭐ 延迟 | ⭐⭐⭐⭐⭐ 立即 |
| **精确度** | ⭐⭐ 模糊 | ⭐⭐⭐⭐⭐ 精确 |
| **性能** | ⭐⭐⭐⭐ 好 | ⭐⭐⭐⭐⭐ 更好 |
| **ASIO 兼容性** | ⭐⭐⭐ 一般 | ⭐⭐⭐⭐⭐ 完全兼容 |
| **代码维护** | ⭐⭐⭐⭐⭐ 简单 | ⭐⭐⭐⭐ 稍复杂 |
| **用户体验** | ⭐⭐⭐ 需要额外检查 | ⭐⭐⭐⭐⭐ 直观 |
| **资源效率** | ⭐⭐⭐ 可能浪费 | ⭐⭐⭐⭐⭐ 高效 |

---

## 🔍 真实场景分析

### 场景 1: 连接突然断开

**方式 1 的行为**：
```cpp
// 连接断开时
srt_epoll_wait 返回：
  read_fds: [sock]
  write_fds: [sock]

// 触发 read_handler
co_await reactor.async_wait_readable(sock);  // 成功返回

// 用户尝试读取
int n = srt_recv(sock, buffer, size);  // ← 这里才发现错误！
if (n < 0) {
    // 处理错误
}
```

**方式 2 的行为**：
```cpp
// 连接断开时
srt_epoll_uwait 返回：
  events[0] = {sock, SRT_EPOLL_ERR}

// 立即检测到错误
if (flags & SRT_EPOLL_ERR) {
    handler(connection_reset_error, flags);
}

// 用户代码
try {
    co_await reactor.async_wait_readable(sock);
} catch (const asio::system_error& e) {
    // ← 直接在这里捕获错误！
}
```

### 场景 2: 同时可读可写

**方式 1 的行为**：
```cpp
// Socket 同时可读可写
read_fds: [sock]
write_fds: [sock]

// 触发两次处理
process_fds(read_fds, ...);   // 触发 read_handler
process_fds(write_fds, ...);  // 触发 write_handler
```

**方式 2 的行为**：
```cpp
// Socket 同时可读可写
events[0] = {sock, SRT_EPOLL_IN | SRT_EPOLL_OUT}

// 在一次处理中完成
if (flags & SRT_EPOLL_IN) { /* 触发 read_handler */ }
if (flags & SRT_EPOLL_OUT) { /* 触发 write_handler */ }
```

---

## 💡 建议的实施计划

### Phase 1: 准备工作
1. 创建分支进行修改
2. 编写新的测试用例验证错误处理
3. 更新文档说明新行为

### Phase 2: 实现
1. 修改 `poll_loop` 使用 `srt_epoll_uwait`
2. 更新错误处理逻辑
3. 确保向后兼容（如果可能）

### Phase 3: 验证
1. 运行所有现有测试
2. 添加错误场景测试
3. 性能基准测试

### Phase 4: 文档
1. 更新 API 文档
2. 添加错误处理示例
3. 更新迁移指南

---

## 结论

**推荐使用方式 2 (srt_epoll_uwait)**，因为：

✅ **更正确** - 立即检测并报告错误  
✅ **更高效** - 避免无效操作  
✅ **更一致** - 符合 ASIO 语义  
✅ **更易用** - 用户代码更简洁  

虽然需要一定的代码修改，但从长远来看，这是更好的选择。

