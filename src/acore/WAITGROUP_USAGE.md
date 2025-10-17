# async_waitgroup - 异步等待组

类似 Go 语言的 `sync.WaitGroup`，用于等待一组异步任务完成。

## 核心 API

### 1. `add(delta)` - 增加计数器

```cpp
wg->add(3);  // 增加 3 个任务
wg->add(1);  // 增加 1 个任务（默认）
```

### 2. `done()` - 完成一个任务

```cpp
wg->done();  // 等同于 wg->add(-1)
```

### 3. `wait()` - 等待所有任务完成

```cpp
co_await wg->wait(asio::use_awaitable);
```

### 4. `wait_for(timeout)` - 带超时的等待

```cpp
bool completed = co_await wg->wait_for(5s, asio::use_awaitable);
if (completed) {
    // 所有任务在超时前完成
} else {
    // 超时了
}
```

## 使用场景

### 场景 1: 等待多个异步任务

```cpp
asio::awaitable<void> process_batch() {
    auto ex = co_await asio::this_coro::executor;
    auto wg = std::make_shared<bcast::async_waitgroup>(ex);
    
    std::vector<std::string> urls = {"url1", "url2", "url3"};
    
    // 启动多个下载任务
    wg->add(urls.size());
    for (const auto& url : urls) {
        asio::co_spawn(ex, [wg, url]() -> asio::awaitable<void> {
            co_await download(url);
            wg->done();
        }, asio::detached);
    }
    
    // 等待所有下载完成
    co_await wg->wait(asio::use_awaitable);
    std::cout << "所有文件下载完成\n";
}
```

### 场景 2: 优雅关闭（graceful shutdown）

```cpp
class Server {
    std::shared_ptr<async_waitgroup> active_requests_;
    
public:
    void handle_request() {
        active_requests_->add(1);
        
        asio::co_spawn(ex_, [this]() -> asio::awaitable<void> {
            // 自动在函数结束时调用 done()
            auto guard = defer([this]() { active_requests_->done(); });
            
            // 处理请求...
            co_await process();
        }, asio::detached);
    }
    
    asio::awaitable<void> shutdown() {
        // 停止接受新请求
        stop_accepting();
        
        // 等待所有活跃请求完成
        std::cout << "等待 " << active_requests_->count() << " 个请求完成...\n";
        co_await active_requests_->wait(asio::use_awaitable);
        
        std::cout << "所有请求已处理，可以安全关闭\n";
    }
};
```

### 场景 3: 分阶段任务协调

```cpp
asio::awaitable<void> pipeline() {
    auto ex = co_await asio::this_coro::executor;
    
    // 阶段 1: 数据获取
    auto fetch_wg = std::make_shared<bcast::async_waitgroup>(ex);
    fetch_wg->add(3);
    
    std::vector<Data> results;
    for (int i = 0; i < 3; ++i) {
        asio::co_spawn(ex, [&results, fetch_wg, i]() -> asio::awaitable<void> {
            auto data = co_await fetch_data(i);
            results.push_back(data);
            fetch_wg->done();
        }, asio::detached);
    }
    
    co_await fetch_wg->wait(asio::use_awaitable);
    std::cout << "阶段 1 完成：数据获取\n";
    
    // 阶段 2: 数据处理
    auto process_wg = std::make_shared<bcast::async_waitgroup>(ex);
    process_wg->add(results.size());
    
    for (const auto& data : results) {
        asio::co_spawn(ex, [process_wg, data]() -> asio::awaitable<void> {
            co_await process_data(data);
            process_wg->done();
        }, asio::detached);
    }
    
    co_await process_wg->wait(asio::use_awaitable);
    std::cout << "阶段 2 完成：数据处理\n";
}
```

### 场景 4: Worker Pool

```cpp
class WorkerPool {
    std::shared_ptr<async_waitgroup> workers_wg_;
    asio::io_context& io_context_;
    bool stopped_ = false;
    
public:
    WorkerPool(asio::io_context& io, int num_workers)
        : io_context_(io)
        , workers_wg_(std::make_shared<bcast::async_waitgroup>(io.get_executor()))
    {
        // 启动 workers
        workers_wg_->add(num_workers);
        for (int i = 0; i < num_workers; ++i) {
            asio::co_spawn(io_, [this, i]() -> asio::awaitable<void> {
                co_await worker_loop(i);
                workers_wg_->done();
            }, asio::detached);
        }
    }
    
    asio::awaitable<void> worker_loop(int worker_id) {
        while (!stopped_) {
            // 从队列获取任务并处理
            auto task = co_await get_task();
            if (task) {
                co_await process(task);
            }
        }
        std::cout << "Worker " << worker_id << " 退出\n";
    }
    
    asio::awaitable<void> shutdown() {
        stopped_ = true;
        
        // 等待所有 workers 退出
        std::cout << "等待 workers 退出...\n";
        co_await workers_wg_->wait(asio::use_awaitable);
        std::cout << "所有 workers 已退出\n";
    }
};
```

### 场景 5: 带超时的批量操作

```cpp
asio::awaitable<void> batch_operation_with_timeout() {
    auto ex = co_await asio::this_coro::executor;
    auto wg = std::make_shared<bcast::async_waitgroup>(ex);
    
    wg->add(10);
    for (int i = 0; i < 10; ++i) {
        asio::co_spawn(ex, [wg, i]() -> asio::awaitable<void> {
            co_await slow_operation(i);
            wg->done();
        }, asio::detached);
    }
    
    // 最多等待 30 秒
    bool completed = co_await wg->wait_for(30s, asio::use_awaitable);
    
    if (completed) {
        std::cout << "所有操作在 30 秒内完成\n";
    } else {
        std::cout << "警告：部分操作超过 30 秒仍未完成\n";
        std::cout << "剩余任务数: " << wg->count() << "\n";
        // 可以选择继续等待或采取其他措施
    }
}
```

## RAII 风格使用

推荐使用 RAII guard 来自动调用 `done()`：

```cpp
class WaitGroupGuard {
public:
    explicit WaitGroupGuard(std::shared_ptr<async_waitgroup> wg)
        : wg_(std::move(wg)) {}
    
    ~WaitGroupGuard() {
        if (wg_) wg_->done();
    }
    
    // 禁止复制
    WaitGroupGuard(const WaitGroupGuard&) = delete;
    WaitGroupGuard& operator=(const WaitGroupGuard&) = delete;
    
private:
    std::shared_ptr<async_waitgroup> wg_;
};

// 使用
asio::awaitable<void> task() {
    WaitGroupGuard guard(wg);  // 自动调用 done()
    
    // ... 执行任务 ...
    
    // guard 析构时自动调用 wg->done()
}
```

或者使用 lambda：

```cpp
auto defer_done = [wg](){ wg->done(); };
auto guard = std::shared_ptr<void>(nullptr, [wg](void*){ wg->done(); });
```

## 注意事项

### 1. 计数不能为负

```cpp
wg->add(5);
for (int i = 0; i < 6; ++i) {  // ⚠️ 错误：调用了 6 次 done()
    wg->done();
}
// 第 6 次 done() 会导致计数变为负数，会被自动设为 0
```

### 2. 所有 `add()` 应该在 `wait()` 之前

```cpp
// ✓ 正确
wg->add(3);
for (...) { spawn_task(); }
co_await wg->wait();

// ✗ 错误：可能导致竞态条件
for (...) { 
    wg->add(1);
    spawn_task(); 
}
co_await wg->wait();  // 可能有些任务还没 add
```

### 3. 避免忘记调用 `done()`

使用 RAII guard 或确保所有路径都调用 `done()`：

```cpp
// ✓ 使用 guard
asio::awaitable<void> safe_task() {
    WaitGroupGuard guard(wg);
    
    if (error) {
        co_return;  // guard 会自动调用 done()
    }
    
    co_await do_work();
    // guard 会自动调用 done()
}

// ✗ 容易遗漏
asio::awaitable<void> unsafe_task() {
    if (error) {
        co_return;  // 忘记调用 done()！
    }
    
    co_await do_work();
    wg->done();  // 只有正常路径调用了
}
```

### 4. 多次等待是安全的

```cpp
// 多个协程可以等待同一个 WaitGroup
asio::co_spawn(ex, [wg]() -> asio::awaitable<void> {
    co_await wg->wait(asio::use_awaitable);
    std::cout << "等待者 1 完成\n";
}, asio::detached);

asio::co_spawn(ex, [wg]() -> asio::awaitable<void> {
    co_await wg->wait(asio::use_awaitable);
    std::cout << "等待者 2 完成\n";
}, asio::detached);

// 当计数归零时，两个等待者都会被唤醒
```

## 性能特性

- **无锁读取**：`count()` 使用原子操作，无需锁
- **Strand 串行化**：所有修改操作在 strand 上串行执行，保证线程安全
- **零拷贝**：handler 使用移动语义，避免不必要的拷贝
- **类型擦除**：使用虚函数实现 handler 存储，支持任意 handler 类型

## 与 Go WaitGroup 的对比

| 特性 | Go sync.WaitGroup | async_waitgroup |
|-----|------------------|-----------------|
| `Add(n)` | ✓ | ✓ `add(n)` |
| `Done()` | ✓ | ✓ `done()` |
| `Wait()` | ✓ 阻塞 | ✓ 异步（co_await） |
| 超时等待 | ✗ | ✓ `wait_for(timeout)` |
| 多个等待者 | ✓ | ✓ |
| 计数归零后 Add | Panic | 忽略（可配置） |
| 线程安全 | ✓ | ✓ |

## 测试

运行测试：

```bash
cd src/acore
./build_test.sh
./build/test_waitgroup
```

测试覆盖：
- 基本功能（多任务等待）
- 批量添加
- 超时等待
- 多个等待者
- 立即完成（计数为0）
- 嵌套 WaitGroup
- RAII guard 模式


