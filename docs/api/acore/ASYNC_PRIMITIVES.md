# 异步同步原语对比

本文档对比各种异步同步原语的使用场景和特性。

## 快速选择指南

| 需求 | 使用工具 |
|------|---------|
| 等待一组任务全部完成 | **async_waitgroup** |
| 限制并发数量 | **async_semaphore** |
| 广播事件给多个等待者 | **async_event** |
| 生产者-消费者队列 | **async_queue** |
| 发布-订阅模式 | **dispatcher** |

## 详细对比

### 1. async_waitgroup - 等待组

**何时使用**: 需要等待一组动态数量的异步任务完成

**特点**:
- 计数器模式：add() 增加，done() 减少，wait() 等待归零
- 支持多个等待者
- 类似 Go 的 `sync.WaitGroup`

**示例**:
```cpp
auto wg = std::make_shared<acore::async_waitgroup>(ex);

// 批量下载文件
wg->add(urls.size());
for (const auto& url : urls) {
    co_spawn(ex, [wg, url]() -> awaitable<void> {
        co_await download(url);
        wg->done();
    }, detached);
}

co_await wg->wait(use_awaitable);
std::cout << "所有文件下载完成\n";
```

**适用场景**:
- ✅ 批量异步操作（下载、上传、处理）
- ✅ 优雅关闭（等待所有请求处理完）
- ✅ 分阶段流水线（一个阶段完成后才开始下一个）
- ✅ Worker Pool 管理

### 2. async_semaphore - 信号量

**何时使用**: 需要限制同时访问某个资源的数量

**特点**:
- 计数器 + 等待队列
- acquire() 减少计数，release() 增加计数
- 用于资源配额管理

**示例**:
```cpp
auto sem = std::make_shared<acore::async_semaphore>(ex, 3);  // 最多 3 个并发

// 限制并发下载数
for (const auto& url : urls) {
    co_spawn(ex, [sem, url]() -> awaitable<void> {
        co_await sem->acquire(use_awaitable);  // 等待空闲槽位
        
        co_await download(url);
        
        sem->release();  // 释放槽位
    }, detached);
}
```

**适用场景**:
- ✅ 限制并发连接数
- ✅ 数据库连接池
- ✅ 限流和背压控制
- ✅ 资源配额管理

### 3. async_event - 事件

**何时使用**: 需要广播通知多个等待者

**特点**:
- 手动重置事件
- notify_all() 唤醒所有等待者
- 类似条件变量（但更简单）

**示例**:
```cpp
auto ready_event = std::make_shared<acore::async_event>(ex);

// 多个 worker 等待启动信号
for (int i = 0; i < 5; ++i) {
    co_spawn(ex, [ready_event, i]() -> awaitable<void> {
        co_await ready_event->wait(use_awaitable);  // 等待信号
        std::cout << "Worker " << i << " 开始工作\n";
    }, detached);
}

// 准备就绪后，唤醒所有 worker
initialize();
ready_event->notify_all();  // 所有 worker 同时开始
```

**适用场景**:
- ✅ 多个 worker 等待启动信号
- ✅ 状态变化通知（如连接状态）
- ✅ 条件同步
- ✅ 屏障同步（barrier）

### 4. async_queue - 队列

**何时使用**: 单个生产者-消费者或多个生产者-单个消费者

**特点**:
- FIFO 队列
- 支持批量操作
- 支持超时读取

**示例**:
```cpp
auto queue = std::make_shared<acore::async_queue<Task>>(io);

// 消费者
co_spawn(ex, [queue]() -> awaitable<void> {
    while (true) {
        auto [ec, task] = co_await queue->async_read_msg(use_awaitable);
        if (ec) break;
        co_await process(task);
    }
}, detached);

// 生产者
queue->push(task);
```

**适用场景**:
- ✅ 任务队列
- ✅ 消息传递
- ✅ 生产者-消费者模式
- ✅ 工作窃取

### 5. dispatcher - 发布订阅

**何时使用**: 一对多广播，多个独立消费者

**特点**:
- 每个订阅者有独立队列
- 支持动态订阅/取消订阅
- 订阅者之间完全隔离

**示例**:
```cpp
auto dispatcher = acore::make_dispatcher<Message>(io);

// 多个订阅者
auto queue1 = dispatcher->subscribe();
auto queue2 = dispatcher->subscribe();

co_spawn(ex, subscriber(queue1), detached);
co_spawn(ex, subscriber(queue2), detached);

// 发布消息（所有订阅者都会收到）
dispatcher->publish(msg);
```

**适用场景**:
- ✅ 事件总线
- ✅ WebSocket 广播
- ✅ 实时消息推送
- ✅ 日志分发

## 组合使用示例

### 示例 1: 限流 + 等待完成

```cpp
// 限流：最多 5 个并发
auto sem = std::make_shared<acore::async_semaphore>(ex, 5);

// 等待所有任务完成
auto wg = std::make_shared<acore::async_waitgroup>(ex);

wg->add(urls.size());
for (const auto& url : urls) {
    co_spawn(ex, [sem, wg, url]() -> awaitable<void> {
        co_await sem->acquire(use_awaitable);  // 限流
        
        co_await download(url);
        
        sem->release();
        wg->done();  // 完成
    }, detached);
}

co_await wg->wait(use_awaitable);  // 等待全部完成
```

### 示例 2: 事件触发 + 任务等待

```cpp
auto start_event = std::make_shared<acore::async_event>(ex);
auto wg = std::make_shared<acore::async_waitgroup>(ex);

wg->add(10);
for (int i = 0; i < 10; ++i) {
    co_spawn(ex, [start_event, wg, i]() -> awaitable<void> {
        co_await start_event->wait(use_awaitable);  // 等待开始信号
        
        co_await do_work(i);
        
        wg->done();
    }, detached);
}

// 所有 worker 就位后，统一开始
start_event->notify_all();

// 等待所有 worker 完成
co_await wg->wait(use_awaitable);
```

### 示例 3: 分阶段流水线

```cpp
auto wg1 = std::make_shared<acore::async_waitgroup>(ex);
auto wg2 = std::make_shared<acore::async_waitgroup>(ex);

// 阶段 1: 数据采集
wg1->add(sources.size());
for (const auto& source : sources) {
    co_spawn(ex, [wg1, source]() -> awaitable<void> {
        co_await fetch(source);
        wg1->done();
    }, detached);
}

co_await wg1->wait(use_awaitable);
std::cout << "阶段 1 完成：数据采集\n";

// 阶段 2: 数据处理
wg2->add(data.size());
for (const auto& item : data) {
    co_spawn(ex, [wg2, item]() -> awaitable<void> {
        co_await process(item);
        wg2->done();
    }, detached);
}

co_await wg2->wait(use_awaitable);
std::cout << "阶段 2 完成：数据处理\n";
```

## 性能对比

| 工具 | 唤醒开销 | 内存占用 | 适合场景 |
|-----|---------|---------|---------|
| **waitgroup** | O(n) waiters | 小 | 少量等待者 |
| **semaphore** | O(1) | 小 | 高频获取/释放 |
| **event** | O(n) waiters | 小 | 低频广播 |
| **queue** | O(1) | O(消息数) | 高吞吐消息传递 |
| **dispatcher** | O(订阅者数) | O(订阅者 × 消息) | 多订阅者广播 |

## 常见模式

### 模式 1: 优雅关闭

```cpp
class Server {
    std::shared_ptr<async_waitgroup> active_requests_;
    std::atomic<bool> shutting_down_{false};
    
public:
    void handle_request() {
        if (shutting_down_) return;
        
        active_requests_->add(1);
        co_spawn(ex_, [this]() -> awaitable<void> {
            auto guard = defer([this]() { active_requests_->done(); });
            co_await process_request();
        }, detached);
    }
    
    awaitable<void> shutdown() {
        shutting_down_ = true;
        
        // 等待所有活跃请求完成
        co_await active_requests_->wait_for(30s, use_awaitable);
        
        // 可以安全关闭
    }
};
```

### 模式 2: Worker Pool

```cpp
class WorkerPool {
    std::shared_ptr<async_semaphore> slots_;  // 控制并发
    std::shared_ptr<async_queue<Task>> tasks_;
    std::shared_ptr<async_waitgroup> workers_;
    
public:
    WorkerPool(asio::io_context& io, int num_workers)
        : slots_(std::make_shared<async_semaphore>(io.get_executor(), num_workers))
        , tasks_(std::make_shared<async_queue<Task>>(io))
        , workers_(std::make_shared<async_waitgroup>(io.get_executor()))
    {
        workers_->add(num_workers);
        for (int i = 0; i < num_workers; ++i) {
            co_spawn(io, worker_loop(), detached);
        }
    }
    
    awaitable<void> worker_loop() {
        while (true) {
            auto [ec, task] = co_await tasks_->async_read_msg(use_awaitable);
            if (ec) break;
            
            co_await slots_->acquire(use_awaitable);
            co_await process(task);
            slots_->release();
        }
        workers_->done();
    }
    
    awaitable<void> shutdown() {
        tasks_->stop();
        co_await workers_->wait(use_awaitable);
    }
};
```

### 模式 3: 超时保护

```cpp
awaitable<void> batch_with_timeout() {
    auto wg = std::make_shared<async_waitgroup>(ex);
    
    wg->add(tasks.size());
    for (auto& task : tasks) {
        co_spawn(ex, [wg, task]() -> awaitable<void> {
            co_await process(task);
            wg->done();
        }, detached);
    }
    
    // 最多等待 30 秒
    bool completed = co_await wg->wait_for(30s, use_awaitable);
    
    if (!completed) {
        std::cerr << "警告：" << wg->count() << " 个任务超时\n";
    }
}
```

## 总结

选择合适的同步原语可以让代码更清晰、更高效：

- **WaitGroup**: 动态数量任务的完成同步
- **Semaphore**: 资源配额和并发控制
- **Event**: 状态广播和多方同步
- **Queue**: 异步消息传递
- **Dispatcher**: 发布订阅模式

在复杂场景中，通常需要组合使用多种原语来实现所需的同步语义。

