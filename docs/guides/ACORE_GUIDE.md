# ACORE 异步组件完整使用指南

## 📋 目录

1. [同步原语](#同步原语)
   - async_mutex
   - async_semaphore
   - async_barrier
2. [事件通知](#事件通知)
   - async_event
   - async_auto_reset_event
3. [计数器](#计数器)
   - async_waitgroup
   - async_latch
4. [消息传递](#消息传递)
   - async_queue
   - dispatcher
5. [定时器](#定时器)
   - async_periodic_timer
   - async_timer
6. [流量控制](#流量控制)
   - async_rate_limiter

---

## 🔒 同步原语

### async_mutex - 异步互斥锁

**何时使用**: 保护共享资源的独占访问

```cpp
#include "acore/async_mutex.hpp"

auto mutex = std::make_shared<async_mutex>(ex);

// RAII 风格（推荐）
{
    auto guard = co_await mutex->async_lock(use_awaitable);
    // ... 临界区代码 ...
}  // 自动解锁

// 带超时
bool acquired = co_await mutex->try_lock_for(5s, use_awaitable);
if (acquired) {
    // 获取锁成功
    mutex->unlock();
}
```

**实际应用**:
- 保护 SRT socket 配置
- 连接池管理
- 统计数据更新

---

### async_semaphore - 异步信号量

**何时使用**: 限制并发数量

```cpp
#include "acore/async_semaphore.hpp"

auto sem = std::make_shared<async_semaphore>(ex, 5);  // 最多 5 个并发

co_await sem->acquire(use_awaitable);
// ... 受限操作 ...
sem->release();
```

**实际应用**:
- 限制并发连接数
- 资源池管理
- 工作队列控制

---

### async_barrier - 同步屏障

**何时使用**: 多个协程需要在同步点等待彼此

```cpp
#include "acore/async_barrier.hpp"

auto barrier = std::make_shared<async_barrier>(ex, 3);  // 3 个参与者

// 每个协程
for (int round = 0; round < 10; ++round) {
    do_phase1();
    co_await barrier->async_arrive_and_wait(use_awaitable);  // 同步点
    
    do_phase2();
    co_await barrier->async_arrive_and_wait(use_awaitable);  // 同步点
}
```

**实际应用**:
- 音视频流同步
- 多阶段数据处理
- 分布式任务协调

---

## 📢 事件通知

### async_event - 手动重置事件（广播）

**何时使用**: 需要通知所有等待者

```cpp
#include "acore/async_event.hpp"

auto event = std::make_shared<async_event>(ex);

// 多个 worker 等待
for (int i = 0; i < 5; ++i) {
    co_spawn(ex, [event]() -> awaitable<void> {
        co_await event->wait(use_awaitable);
        // 所有人同时被唤醒
    }, detached);
}

event->notify_all();  // 广播
event->reset();       // 重置（供下次使用）
```

**实际应用**:
- 启动信号（所有 worker 同时开始）
- 状态变化通知
- 关闭信号

---

### async_auto_reset_event - 自动重置事件（单播）

**何时使用**: 任务分发，每次只通知一个

```cpp
#include "acore/async_auto_reset_event.hpp"

auto event = std::make_shared<async_auto_reset_event>(ex);

// Worker 池（每次只有一个被唤醒）
for (int i = 0; i < 5; ++i) {
    co_spawn(ex, [event]() -> awaitable<void> {
        while (true) {
            co_await event->wait(use_awaitable);
            process_one_task();  // 只有一个 worker 处理
        }
    }, detached);
}

event->notify();  // 唤醒一个 worker
```

**实际应用**:
- 任务队列
- 请求分发
- 单次响应

---

## 🔢 计数器

### async_waitgroup - 等待组

**何时使用**: 等待动态数量的任务完成

```cpp
#include "acore/async_waitgroup.hpp"

auto wg = std::make_shared<async_waitgroup>(ex);

// 动态添加任务
for (int i = 0; i < 10; ++i) {
    wg->add(1);
    co_spawn(ex, [wg]() -> awaitable<void> {
        do_work();
        wg->done();
    }, detached);
}

co_await wg->wait(use_awaitable);  // 等待所有完成
```

**实际应用**:
- 等待一批异步任务
- 并行下载
- 分布式任务协调

---

### async_latch - 一次性计数器

**何时使用**: 等待固定数量的任务（简化版 waitgroup）

```cpp
#include "acore/async_latch.hpp"

auto latch = std::make_shared<async_latch>(ex, 3);  // 固定 3 个任务

for (int i = 0; i < 3; ++i) {
    co_spawn(ex, [latch]() -> awaitable<void> {
        initialize();
        latch->count_down();  // 报告完成
    }, detached);
}

co_await latch->wait(use_awaitable);
// 所有初始化完成，开始应用
```

**实际应用**:
- 启动屏障
- 等待初始化
- 关闭协调

---

## 📬 消息传递

### async_queue - 异步队列

**何时使用**: 生产者-消费者模式

```cpp
#include "acore/async_queue.hpp"

auto queue = std::make_shared<async_queue<Task>>(io_ctx);

// 生产者
queue->push(task);

// 消费者
auto [ec, task] = co_await queue->async_read_msg(use_awaitable);
if (!ec) {
    process(task);
}

// 批量读取（高性能）
auto [ec, tasks] = co_await queue->async_read_msgs(10, use_awaitable);
```

**实际应用**:
- SRT 数据包接收队列
- 任务队列
- 日志收集

---

### dispatcher - 发布订阅

**何时使用**: 一对多广播，每个订阅者独立处理

```cpp
#include "acore/dispatcher.hpp"

auto disp = make_dispatcher<Message>(io_ctx);

// 订阅
auto queue1 = disp->subscribe();
auto queue2 = disp->subscribe();

// 消费者
co_spawn(ex, [queue1]() -> awaitable<void> {
    while (true) {
        auto [ec, msg] = co_await queue1->async_read_msg(use_awaitable);
        process(msg);
    }
}, detached);

// 发布（所有订阅者都收到）
disp->publish(message);
```

**实际应用**:
- 事件总线
- WebSocket 广播
- 多订阅者通知

---

## ⏱️ 定时器

### async_periodic_timer - 周期定时器

**何时使用**: 需要周期性执行任务

```cpp
#include "acore/async_periodic_timer.hpp"

auto timer = std::make_shared<async_periodic_timer>(ex, 5s);

while (true) {
    co_await timer->async_next(use_awaitable);
    send_heartbeat();  // 每 5 秒执行
}

// 动态控制
timer->pause();     // 暂停
timer->resume();    // 恢复
timer->set_period(10s);  // 修改周期
```

**实际应用**:
- 心跳包发送
- 统计上报
- 健康检查
- 定期轮询

---

### async_timer - 一次性定时器

**何时使用**: 简单的延迟执行

```cpp
#include "acore/async_periodic_timer.hpp"

async_timer timer(ex);

// 延迟 5 秒
co_await timer.async_wait_for(5s, use_awaitable);

// 等待到指定时间
auto time_point = std::chrono::steady_clock::now() + 10s;
co_await timer.async_wait_until(time_point, use_awaitable);
```

**实际应用**:
- 超时保护
- 延迟重试
- 定时任务

---

## 🚦 流量控制

### async_rate_limiter - 速率限制器

**何时使用**: 需要限制操作频率

```cpp
#include "acore/async_rate_limiter.hpp"

// 限制：每秒 100 个请求，允许突发 200 个
auto limiter = std::make_shared<async_rate_limiter>(ex, 100, 1s, 200);

// 获取 1 个令牌
co_await limiter->async_acquire(use_awaitable);
send_request();

// 按大小消耗（带宽限制）
co_await limiter->async_acquire(packet_size, use_awaitable);
send_packet(data, packet_size);

// 非阻塞检查
bool acquired = co_await limiter->async_try_acquire(use_awaitable);
if (acquired) {
    send_request();
}
```

**实际应用**:
- API 调用频率限制
- 带宽控制
- 连接速率限制
- QoS 流量整形

---

## 🎯 组件选择指南

### 需要保护共享资源？
✅ 使用 `async_mutex`

### 需要限制并发数？
✅ 使用 `async_semaphore`

### 需要多阶段同步？
✅ 使用 `async_barrier`

### 需要广播通知？
✅ 使用 `async_event`（手动重置）

### 需要任务分发？
✅ 使用 `async_auto_reset_event`（自动重置）或 `async_queue`

### 需要等待任务完成？
- 动态任务 → `async_waitgroup`
- 固定任务 → `async_latch`

### 需要消息传递？
- 单个消费者 → `async_queue`
- 多个订阅者 → `dispatcher`

### 需要周期性执行？
✅ 使用 `async_periodic_timer`

### 需要限制速率？
✅ 使用 `async_rate_limiter`

---

## 🔧 最佳实践

### 1. 使用 shared_ptr 管理生命周期
```cpp
✅ auto mutex = std::make_shared<async_mutex>(ex);
❌ async_mutex mutex(ex);  // 不要这样
```

### 2. 使用 RAII
```cpp
✅ auto guard = co_await mutex->async_lock(use_awaitable);
✅ auto timer = make_shared<async_periodic_timer>(ex, 5s);
```

### 3. 合理设置超时
```cpp
✅ co_await mutex->try_lock_for(5s, use_awaitable);
✅ co_await queue->async_read_msg_with_timeout(10s, use_awaitable);
```

### 4. 处理错误
```cpp
auto [ec, msg] = co_await queue->async_read_msg(asio::as_tuple(use_awaitable));
if (ec) {
    // 处理错误
}
```

---

## 📚 相关文档

- **完整 API**: [src/acore/README.md](../../src/acore/README.md)
- **测试示例**: [tests/acore/](../../tests/acore/)
- **代码示例**: [examples/acore/](../../examples/acore/)
- **设计文档**: [../design/](../design/)

---

**指南版本**: 2.0  
**最后更新**: 2025-10-19  
**适用版本**: ACORE 1.0+

