# 代码审查总结

本文档总结了针对 acore 库进行的三轮 Linus 风格代码审查及相应的改进。

## 第一轮审查：核心设计问题

### 发现的问题
1. **async_waitgroup.hpp**: atomic + strand 混用，设计混乱
2. **async_queue.hpp**: 双 strand 设计导致性能问题
3. **整体**: atomic 使用策略不一致

### 实施的改进

#### 1. async_waitgroup.hpp
- ✅ **移除 atomic count_**：改为完全由 strand 保护的普通变量
- ✅ **add() 和 done() 改为异步**：符合异步编程模式，消除复杂的双重检查逻辑
- ✅ **移除 reset() 和 close() 方法**：保持 WaitGroup 的核心语义（一次性使用）
- ✅ **count() 改为 async_count()**：避免给用户虚假的线程安全感

**改进前**：
```cpp
std::atomic<int64_t> count_{0};
void add(int64_t delta) {
    // 同步更新 + 异步唤醒 + 双重检查
    int64_t old_count = count_.fetch_add(delta, std::memory_order_acq_rel);
    // ...复杂的竞态处理
}
```

**改进后**：
```cpp
int64_t count_{0};  // 仅在 strand 内访问
void add(int64_t delta) {
    asio::post(strand_, [this, self = shared_from_this(), delta]() {
        count_ += delta;
        if (count_ == 0) {
            notify_all_waiters();
        }
    });
}
```

#### 2. async_semaphore.hpp
- ✅ **添加可选的 strand 参数**：支持外部提供 strand，用于性能优化
- ✅ **保留内部 strand 版本**：保持向后兼容和独立使用场景

**新增构造函数**：
```cpp
// 使用外部 strand（高性能）
explicit async_semaphore(asio::strand<executor_type> strand, size_t initial_count = 0);
```

#### 3. async_queue.hpp
- ✅ **使用共享 strand**：queue 和 semaphore 共享同一个 strand
- ✅ **消除跨 strand 开销**：semaphore 回调直接访问队列，无需额外 post
- ✅ **性能提升**：减少一次异步调度延迟

**改进前**：
```cpp
async_semaphore semaphore_(io_context.get_executor(), 0);  // 独立 strand
// 每次读取需要在两个 strand 间切换
```

**改进后**：
```cpp
async_semaphore semaphore_(strand_, 0);  // 共享 strand
// 回调直接在同一个 strand 上执行
```

---

## 第二轮审查：设计细节和文档

### 发现的问题
1. **async_semaphore.hpp**: owns_strand_ 字段无用
2. **所有类**: 缺少显式删除的拷贝/移动构造函数
3. **async_queue.hpp**: handler 执行上下文文档不清晰
4. **handler_traits.hpp**: cancellable_void_handler_base 的逻辑需要明确

### 实施的改进

#### 1. 删除无用成员
- ✅ **移除 owns_strand_**：不影响行为的字段被删除

#### 2. 显式删除拷贝和移动
为所有使用 `enable_shared_from_this` 的类添加：
```cpp
// 禁止拷贝和移动（设计上通过 shared_ptr 使用）
async_waitgroup(const async_waitgroup&) = delete;
async_waitgroup& operator=(const async_waitgroup&) = delete;
async_waitgroup(async_waitgroup&&) = delete;
async_waitgroup& operator=(async_waitgroup&&) = delete;
```

应用到：
- ✅ async_waitgroup
- ✅ async_semaphore
- ✅ async_queue
- ✅ async_event
- ✅ dispatcher

#### 3. 改进文档

**async_count() 的设计说明**：
```cpp
/**
 * @brief 异步获取当前计数
 * 
 * 设计说明：
 * - 使用异步接口而非同步的 count() 方法
 * - 原因：同步方法返回的值可能在下一时刻就失效（竞态条件）
 * - 异步接口强制用户意识到这是一个时间点的快照，仅在回调时刻有效
 * - 这避免了给用户虚假的线程安全感
 */
```

**handler 执行上下文警告**：
```cpp
/**
 * 重要：completion handler 在内部 strand 上执行，而不是在调用者的 executor 上。
 * 如果你需要在特定的 executor（如协程的 strand）上执行，请在 handler 中
 * 使用 asio::post 切换到目标 executor。
 */
```

#### 4. 改进 handler_traits.hpp
```cpp
void invoke() {
    // 注意：如果已取消（inner_ 为空），不执行
    // 这是合法的：cancel() 后的 invoke() 调用会被静默忽略
    if (inner_) {
        inner_->invoke();
    }
}
```

---

## 第三轮审查：性能和最佳实践

### 发现的问题
1. **async_queue.hpp**: 成员声明顺序与初始化顺序不一致
2. **示例代码**: 捕获语义不是最佳实践
3. **文档**: 缺少性能指南

### 实施的改进

#### 1. 修复成员声明顺序
确保 `strand_` 在 `semaphore_` 之前声明：

**改进前**：
```cpp
asio::strand<asio::any_io_executor> strand_;
async_semaphore semaphore_;
std::deque<T> queue_;
```

**改进后**：
```cpp
asio::strand<asio::any_io_executor> strand_;  // 必须在 semaphore_ 之前声明
std::deque<T> queue_;
bool stopped_;
async_semaphore semaphore_;  // 最后初始化
```

#### 2. 改进示例代码
**改进前**：
```cpp
asio::post(my_strand, [ec, msg = std::move(msg)]() {
    process(msg);  // msg 已经 move 了
});
```

**改进后**：
```cpp
asio::post(my_strand, [ec, msg = std::move(msg)]() mutable {
    process(std::move(msg));  // 正确的 move 语义
});
```

#### 3. 添加性能指南

**async_waitgroup.hpp**：
```cpp
/**
 * 性能建议：
 * - 每次调用 add() 都会 post 一个任务到 strand
 * - 如果要启动 N 个任务，使用 add(N) 比调用 N 次 add(1) 更高效
 * - 批量操作可以减少 post 开销和 lambda 对象创建
 */
```

**dispatcher.hpp**：
```cpp
/**
 * 性能注意事项：
 * - 消息会被复制给每个订阅者（对于小消息通常很快）
 * - 对于大消息（如大buffer），建议将消息类型定义为 shared_ptr<LargeData>
 * - 这样只会复制指针，避免深拷贝
 * - 两级异步分发（dispatcher strand + 各队列strand）
 * - 高吞吐场景建议使用批量操作
 */
```

**async_semaphore.hpp**：
```cpp
/**
 * 使用场景：当 semaphore 独立使用时
 * vs
 * 使用场景：当 semaphore 与其他组件（如 async_queue）共享 strand 时
 */
```

---

## 总结

### 关键改进点

1. **设计简化**
   - 移除 atomic + strand 混用模式
   - 统一使用 strand 进行同步
   - 删除非核心功能（reset, close）

2. **性能优化**
   - 共享 strand 消除跨 strand 开销
   - 批量操作减少调度开销
   - 文档指导用户进行性能优化

3. **代码质量**
   - 显式删除拷贝/移动构造
   - 成员声明顺序一致
   - 示例代码遵循最佳实践

4. **文档完善**
   - 明确设计决策的理由
   - 警告执行上下文问题
   - 提供性能指南

### Atomic 使用原则

经过审查确立的明确规则：
- ✅ **允许使用**：需要在 strand 外立即返回的值（如 ID 生成）
- ❌ **不允许使用**：可以在 strand 内访问的共享状态
- 📝 **必须文档化**：每个 atomic 变量都必须注释说明为什么需要它

### 性能权衡

| 方案 | 优点 | 缺点 | 使用场景 |
|------|------|------|----------|
| 共享 strand | 低延迟，高吞吐 | handler 在内部 executor 执行 | async_queue |
| 独立 strand | 模块独立，executor 隔离 | 跨 strand 开销 | 通用组件 |
| 批量操作 | 减少调度开销 | API 复杂度增加 | 高吞吐场景 |

### 最终状态

所有代码：
- ✅ 无 linter 错误
- ✅ 设计一致性
- ✅ 文档完善
- ✅ 性能优化
- ✅ 最佳实践

