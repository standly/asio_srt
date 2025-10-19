# ACORE - 异步核心组件库

一套完整的基于 ASIO 和 C++20 协程的异步编程原语。

## 📚 组件列表

### 🔒 同步原语

#### 1. async_mutex
**异步互斥锁**
- ✅ RAII 风格的锁守卫（async_lock_guard）
- ✅ 支持超时锁定
- ✅ 线程安全（使用 strand）
- 📝 [测试用例](test_async_mutex.cpp)

```cpp
auto mutex = std::make_shared<async_mutex>(ex);
auto guard = co_await mutex->async_lock(use_awaitable);
// ... critical section ...
// guard 析构时自动解锁
```

---

#### 2. async_semaphore
**异步信号量**
- ✅ 基于计数的并发控制
- ✅ 支持可取消等待
- ✅ 批量获取/释放
- 📝 [实现代码](async_semaphore.hpp)

```cpp
auto sem = std::make_shared<async_semaphore>(ex, 5);  // 最多 5 个并发
co_await sem->acquire(use_awaitable);
// ... work ...
sem->release();
```

---

#### 3. async_barrier
**多阶段同步屏障**
- ✅ 多个协程在同步点等待彼此
- ✅ 可重用（支持多轮同步）
- ✅ arrive_and_wait / arrive / wait 分离
- 📝 [测试用例](test_async_barrier.cpp)

```cpp
auto barrier = std::make_shared<async_barrier>(ex, 3);  // 3 个参与者

for (int i = 0; i < 3; ++i) {
    co_spawn(ex, [barrier]() -> awaitable<void> {
        prepare_phase1();
        co_await barrier->async_arrive_and_wait(use_awaitable);  // 同步点 1
        
        prepare_phase2();
        co_await barrier->async_arrive_and_wait(use_awaitable);  // 同步点 2
    }, detached);
}
```

---

### 📢 事件通知

#### 4. async_event
**手动重置事件（广播）**
- ✅ notify_all() 唤醒所有等待者
- ✅ 手动 reset()
- ✅ 支持超时等待
- 📝 [实现代码](async_event.hpp)

```cpp
auto event = std::make_shared<async_event>(ex);

// 多个等待者
for (int i = 0; i < 5; ++i) {
    co_spawn(ex, [event]() -> awaitable<void> {
        co_await event->wait(use_awaitable);
        // 所有等待者同时被唤醒
    }, detached);
}

event->notify_all();  // 广播
```

---

#### 5. async_auto_reset_event
**自动重置事件（单播）**
- ✅ notify() 只唤醒一个等待者
- ✅ 自动重置（无需手动 reset）
- ✅ 适合任务分发
- 📝 [测试用例](test_async_auto_reset_event.cpp)

```cpp
auto event = std::make_shared<async_auto_reset_event>(ex);

// Worker 池
for (int i = 0; i < 5; ++i) {
    co_spawn(ex, [event]() -> awaitable<void> {
        while (true) {
            co_await event->wait(use_awaitable);  // 等待任务
            process_task();  // 只有一个 worker 被唤醒
        }
    }, detached);
}

event->notify();  // 唤醒一个 worker
```

---

### 📊 计数器

#### 6. async_waitgroup
**等待组（类似 Go sync.WaitGroup）**
- ✅ 动态计数管理（add/done）
- ✅ 支持超时等待
- ✅ 线程安全
- 📝 [实现代码](async_waitgroup.hpp)

```cpp
auto wg = std::make_shared<async_waitgroup>(ex);

wg->add(3);  // 3 个任务

for (int i = 0; i < 3; ++i) {
    co_spawn(ex, [wg]() -> awaitable<void> {
        do_work();
        wg->done();
    }, detached);
}

co_await wg->wait(use_awaitable);  // 等待所有任务完成
```

---

#### 7. async_latch
**一次性计数器**
- ✅ 只能倒计数（更简单）
- ✅ 一次性使用（不可重置）
- ✅ 适合启动屏障
- 📝 [测试用例](test_async_latch.cpp)

```cpp
auto latch = std::make_shared<async_latch>(ex, 3);

for (int i = 0; i < 3; ++i) {
    co_spawn(ex, [latch]() -> awaitable<void> {
        initialize();
        latch->count_down();  // 报告就绪
    }, detached);
}

co_await latch->wait(use_awaitable);  // 等待所有初始化完成
start_application();
```

---

### 📬 消息传递

#### 8. async_queue
**异步消息队列**
- ✅ 单向队列（push/read）
- ✅ 支持批量操作
- ✅ 支持超时读取
- 📝 [实现代码](async_queue.hpp)

```cpp
auto queue = std::make_shared<async_queue<Task>>(io_ctx);

// 生产者
queue->push(task);

// 消费者
auto [ec, task] = co_await queue->async_read_msg(use_awaitable);
process(task);
```

---

#### 9. dispatcher
**发布-订阅模式**
- ✅ 一对多广播
- ✅ 每个订阅者独立队列
- ✅ 动态订阅/取消订阅
- 📝 [实现代码](dispatcher.hpp)

```cpp
auto disp = make_dispatcher<Message>(io_ctx);

// 订阅
auto queue1 = disp->subscribe();
auto queue2 = disp->subscribe();

// 发布（所有订阅者都收到）
disp->publish(msg);

// 读取
auto [ec, msg] = co_await queue1->async_read_msg(use_awaitable);
```

---

### ⏱️ 定时器

#### 10. async_periodic_timer
**周期性定时器**
- ✅ 自动重置（无需手动 expires_after）
- ✅ 支持暂停/恢复
- ✅ 动态修改周期
- 📝 [测试用例](test_async_periodic_timer.cpp)

```cpp
auto timer = std::make_shared<async_periodic_timer>(ex, 5s);

while (true) {
    co_await timer->async_next(use_awaitable);
    send_heartbeat();  // 每 5 秒执行一次
}
```

---

#### 11. async_timer
**一次性定时器**
- ✅ 简单的延迟执行
- ✅ 支持 wait_for / wait_until
- 📝 [实现代码](async_periodic_timer.hpp)

```cpp
async_timer timer(ex);
co_await timer.async_wait_for(5s, use_awaitable);
// 5 秒后继续
```

---

### 🚦 流量控制

#### 12. async_rate_limiter
**速率限制器（令牌桶算法）**
- ✅ 基于令牌桶的流量控制
- ✅ 支持突发流量
- ✅ 可变令牌消耗（按大小限流）
- 📝 [测试用例](test_async_rate_limiter.cpp)

```cpp
// 限制：每秒 100 个请求，允许突发 200 个
auto limiter = std::make_shared<async_rate_limiter>(ex, 100, 1s, 200);

// 获取令牌
co_await limiter->async_acquire(use_awaitable);
send_request();

// 按大小消耗（如带宽限制）
co_await limiter->async_acquire(packet_size, use_awaitable);
send_packet(packet);
```

---

## 🎯 使用场景对照表

| 需求 | 推荐组件 |
|------|---------|
| 保护共享资源 | `async_mutex` |
| 限制并发数 | `async_semaphore` |
| 多阶段同步 | `async_barrier` |
| 广播通知 | `async_event` |
| 任务分发 | `async_auto_reset_event` |
| 等待任务完成 | `async_waitgroup` 或 `async_latch` |
| 消息队列 | `async_queue` |
| 发布-订阅 | `dispatcher` |
| 周期性任务 | `async_periodic_timer` |
| 延迟执行 | `async_timer` |
| 流量限制 | `async_rate_limiter` |

---

## 📦 组件对比

### async_waitgroup vs async_latch

| 特性 | async_waitgroup | async_latch |
|------|----------------|-------------|
| 计数管理 | add() + done() | 只能 count_down() |
| 动态调整 | ✅ 支持 | ❌ 不支持 |
| 可重用 | ✅ 可重用 | ❌ 一次性 |
| 复杂度 | 稍高 | 更简单 |
| 使用场景 | 动态任务管理 | 固定数量任务 |

---

### async_event vs async_auto_reset_event

| 特性 | async_event | async_auto_reset_event |
|------|------------|----------------------|
| 唤醒方式 | notify_all()（广播） | notify()（单播） |
| 重置 | 手动 reset() | 自动重置 |
| 使用场景 | 状态变化通知 | 任务队列 |

---

### async_queue vs dispatcher

| 特性 | async_queue | dispatcher |
|------|------------|-----------|
| 模式 | 单一消费者 | 多个订阅者 |
| 队列 | 共享队列 | 每个订阅者独立队列 |
| 使用场景 | 生产者-消费者 | 发布-订阅 |

---

## 🔧 构建测试

### 编译测试

```bash
cd build
cmake .. -DBUILD_ACORE_TESTS=ON
make

# 运行所有测试
ctest

# 或单独运行
./src/acore/test_async_mutex
./src/acore/test_async_periodic_timer
./src/acore/test_async_rate_limiter
./src/acore/test_async_barrier
./src/acore/test_async_auto_reset_event
./src/acore/test_async_latch
```

---

## 📖 设计原则

1. **线程安全** - 所有组件使用 strand 保证线程安全
2. **协程友好** - 支持 C++20 协程和 co_await
3. **RAII** - 自动管理资源生命周期
4. **零拷贝** - 尽可能使用移动语义
5. **详细文档** - 每个组件都有完整的注释和使用示例

---

## 🎓 学习资源

### 示例代码
- [examples/acore/](../../examples/acore/) - 实际使用示例

### 测试代码
- 每个组件的测试文件都包含 8-10 个测试用例
- 覆盖基本功能、并发场景、边界情况、压力测试

### API 文档
- [docs/api/acore/](../../docs/api/acore/) - 详细 API 文档

---

## 📊 性能特点

- **低延迟** - 基于 ASIO 的高效事件循环
- **可扩展** - 支持大量并发操作
- **低开销** - Header-only 库，无额外依赖
- **现代 C++** - 充分利用 C++20 特性

---

## 🤝 依赖

- C++20 编译器（支持协程）
- ASIO 1.18+ (header-only)

---

## 📄 许可证

与项目主许可证相同。

---

**版本**: 1.0  
**最后更新**: 2025-10-19

