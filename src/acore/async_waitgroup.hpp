//
// Async WaitGroup - 类似 Go 的 sync.WaitGroup
//
#pragma once

#include <asio.hpp>
#include <deque>
#include <memory>
#include <atomic>
#include <stdexcept>
#include <cassert>
#include "handler_traits.hpp"

namespace acore {

/**
 * @brief 异步等待组 - 类似 Go 的 sync.WaitGroup
 * 
 * 功能：
 * 1. add(n) - 增加计数器
 * 2. done() - 减少计数器（相当于 add(-1)）
 * 3. wait() - 等待计数器变为 0
 * 
 * 特性：
 * - 线程安全（使用 strand）
 * - 支持多个等待者
 * - 支持超时等待
 * - 支持协程（co_await）
 * 
 * 使用场景：
 * - 等待一组异步任务全部完成
 * - 协调多个 worker 的生命周期
 * - 资源清理前确保所有操作完成
 * 
 * 正确的使用模式：
 * @code
 * auto wg = std::make_shared<async_waitgroup>(io_context);
 * 
 * // ✅ 正确：先 add，再启动任务
 * wg->add(3);  // (1) 先增加计数
 * for (int i = 0; i < 3; ++i) {
 *     asio::co_spawn(io_context, [wg, i]() -> asio::awaitable<void> {
 *         // ... 做一些异步工作 ...
 *         wg->done();  // (2) 任务完成时减少计数
 *     }, asio::detached);
 * }
 * 
 * // (3) 等待所有任务完成
 * co_await wg->wait(asio::use_awaitable);
 * std::cout << "所有任务完成！\n";
 * @endcode
 * 
 * 错误的使用模式：
 * @code
 * // ❌ 错误：先启动任务，后 add
 * for (int i = 0; i < 3; ++i) {
 *     asio::co_spawn(io_context, [wg]() -> asio::awaitable<void> {
 *         wg->done();  // ❌ 可能在 add() 前执行，导致负数计数
 *     }, asio::detached);
 * }
 * wg->add(3);  // ❌ 太晚了，done() 可能已经执行
 * @endcode
 */
class async_waitgroup : public std::enable_shared_from_this<async_waitgroup> {
private:
    using executor_type = asio::any_io_executor;

    asio::strand<executor_type> strand_;
    std::deque<std::unique_ptr<detail::void_handler_base>> waiters_;  // 仅在 strand 内访问
    
    /**
     * count_ 使用 atomic 的设计说明：
     * 
     * 为什么使用 atomic：
     * - add()/done() 必须同步更新计数（立即生效）
     * - 如果 add() 是异步的，会导致严重bug：
     *   wg->add(3);              // 如果异步，post 到 strand
     *   co_await wg->wait(...);  // 也 post 到 strand，可能先执行！
     *   // Bug: wait 看到 count=0，错误地立即返回
     * - Go 的 sync.WaitGroup.Add() 也是同步的，这是正确的语义
     * 
     * 为什么需要双重检查（见 add() 方法）：
     * - count 可能经历多次 0 转换：N -> 0 -> M -> 0
     * - 每次到 0 都会 post 一个 notify 任务
     * - 双重检查确保只有 count 真正为 0 时才唤醒 waiters
     * 
     * atomic 和 strand 的分工：
     * - count_: 需要同步更新 + 跨线程访问 -> atomic
     * - waiters_: 只在异步上下文中修改 -> strand 保护
     * - 这不是"混用"，而是"各司其职"
     */
    std::atomic<int64_t> count_{0};

public:
    // 禁止拷贝和移动（设计上通过 shared_ptr 使用）
    async_waitgroup(const async_waitgroup&) = delete;
    async_waitgroup& operator=(const async_waitgroup&) = delete;
    async_waitgroup(async_waitgroup&&) = delete;
    async_waitgroup& operator=(async_waitgroup&&) = delete;

    /**
     * @brief 构造函数（创建内部 strand）
     * 
     * @param ex executor（通常是 io_context.get_executor()）
     * @param initial_count 初始计数（默认为 0）
     */
    explicit async_waitgroup(executor_type ex, int64_t initial_count = 0) 
        : strand_(asio::make_strand(ex))
        , count_(initial_count)
    {
        if (initial_count < 0) {
            throw std::invalid_argument("initial_count cannot be negative");
        }
    }
    
    /**
     * @brief 构造函数（使用外部 strand）
     * 
     * 使用场景：当 waitgroup 与其他组件共享 strand 时
     * 
     * @param strand 外部提供的 strand
     * @param initial_count 初始计数（默认为 0）
     */
    explicit async_waitgroup(asio::strand<executor_type> strand, int64_t initial_count = 0)
        : strand_(strand)
        , count_(initial_count)
    {
        if (initial_count < 0) {
            throw std::invalid_argument("initial_count cannot be negative");
        }
    }

    /**
     * @brief 增加计数器（同步更新，异步唤醒）
     * 
     * @param delta 增加的数量（可以为负数，但结果不能为负）
     * 
     * 关键设计决策：
     * - 计数更新是**同步的**（立即生效，使用 atomic）
     * - 唤醒操作是**异步的**（post 到 strand）
     * 
     * 为什么 add() 必须同步：
     * @code
     * wg->add(3);              // 必须立即更新 count = 3
     * spawn_tasks();           // 启动3个任务
     * co_await wg->wait(...);  // 期望等待这3个任务完成
     * 
     * // 如果 add() 是异步的（post 到 strand）：
     * wg->add(3);              // post，可能延迟执行
     * co_await wg->wait(...);  // 也 post，可能先执行
     * // Bug: wait 看到 count=0，错误地立即返回！
     * @endcode
     * 
     * 典型用法：
     * - 在启动异步任务前调用 add(1) 或 add(N)
     * - 任务完成时调用 done()
     * 
     * 错误处理：
     * - 如果计数变为负数，会触发断言（debug）或恢复为0（release）
     */
    void add(int64_t delta = 1) {
        if (delta == 0) return;
        
        // 同步更新计数（使用 atomic，立即生效）
        // memory_order_acq_rel: 比 relaxed 更安全，性能差异可忽略
        // - Release: 发布之前的写入（虽然 WaitGroup 本身不依赖此特性）
        // - Acquire: 获取最新值
        // - 也可以用 relaxed，但 acq_rel 对将来的扩展更安全
        int64_t old_count = count_.fetch_add(delta, std::memory_order_acq_rel);
        int64_t new_count = old_count + delta;
        
        if (new_count < 0) {
            // 错误：计数变为负数（done()调用次数超过add()）
            count_.store(0, std::memory_order_release);
            // 不使用 assert（Release 模式会被编译掉），而是抛出异常
            // 这是内部不变量错误，表明用户使用不当
            throw std::logic_error("async_waitgroup: negative counter (done() called more times than add())");
        }
        
        // 如果计数归零，异步唤醒所有等待者
        if (new_count == 0) {
            asio::post(strand_, [this, self = shared_from_this()]() {
                // 双重检查：性能优化，避免不必要的操作
                // 
                // 为什么需要：
                // - count 可能在 post(notify) 和执行之间发生变化
                // - 如果 count 已经不是 0（有新任务添加），不应该唤醒
                // - 虽然错误唤醒不会破坏正确性（waiters可能已空），
                //   但会浪费CPU（遍历空队列）
                // 
                // 示例：
                //   T1: done() -> count: 1->0 -> post(notify_1)
                //   T2: add(10) -> count: 0->10 (新任务)
                //   T3: notify_1 执行，count=10 ← 检查避免无用的唤醒
                if (count_.load(std::memory_order_acquire) == 0) {
                    notify_all_waiters();
                }
            });
        }
    }

    /**
     * @brief 减少计数器（等同于 add(-1)）
     * 
     * 任务完成时调用
     */
    void done() {
        add(-1);
    }

    /**
     * @brief 等待计数器归零
     * 
     * 如果计数已经为 0，立即完成
     * 否则，等待直到计数变为 0
     * 
     * 用法：
     * @code
     * co_await wg->wait(asio::use_awaitable);
     * @endcode
     */
    template<typename CompletionToken = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>>
    auto wait(CompletionToken&& token = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>{}) {
        return asio::async_initiate<CompletionToken, void()>(
            [this, self = shared_from_this()](auto handler) {
                asio::post(strand_, [this, self, handler = std::move(handler)]() mutable {
                    // 使用 atomic load 读取 count
                    if (count_.load(std::memory_order_acquire) == 0) {
                        // 计数已为 0，立即完成
                        std::move(handler)();
                    } else {
                        // 计数不为 0，加入等待队列
                        waiters_.push_back(
                            std::make_unique<detail::void_handler_impl<decltype(handler)>>(std::move(handler))
                        );
                    }
                });
            },
            token
        );
    }

    /**
     * @brief 带超时的等待
     * 
     * @param timeout 超时时长
     * @return true - 计数在超时前归零
     *         false - 等待超时
     * 
     * 用法：
     * @code
     * bool completed = co_await wg->wait_for(5s, asio::use_awaitable);
     * if (completed) {
     *     std::cout << "所有任务完成\n";
     * } else {
     *     std::cout << "超时！\n";
     * }
     * @endcode
     */
    template<typename Rep, typename Period, typename CompletionToken = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>>
    auto wait_for(
        std::chrono::duration<Rep, Period> timeout,
        CompletionToken&& token = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>{})
    {
        return asio::async_initiate<CompletionToken, void(bool)>(
            [this, timeout, self = shared_from_this()](auto handler) {
                // 完成标志：确保 handler 只被调用一次
                auto completed = std::make_shared<std::atomic<bool>>(false);
                auto timer = std::make_shared<asio::steady_timer>(strand_.get_inner_executor());
                
                // 将 handler 类型擦除并存储在 shared_ptr 中
                auto handler_ptr = std::make_shared<std::unique_ptr<detail::bool_handler_base>>(
                    std::make_unique<detail::bool_handler_impl<decltype(handler)>>(std::move(handler))
                );
                
                // 超时定时器
                timer->expires_after(timeout);
                timer->async_wait([completed, handler_ptr](const std::error_code& ec) mutable {
                    if (!ec && !completed->exchange(true, std::memory_order_acq_rel)) {
                        // 超时触发
                        if (*handler_ptr) {
                            auto h = std::move(*handler_ptr);
                            h->invoke(false);  // false = 超时
                        }
                    }
                });
                
                // 等待计数归零
                asio::post(strand_, [this, self, completed, timer, handler_ptr]() mutable {
                    // 使用 atomic load 读取 count
                    if (count_.load(std::memory_order_acquire) == 0) {
                        // 计数已为 0
                        if (!completed->exchange(true, std::memory_order_relaxed)) {
                            timer->cancel();
                            if (*handler_ptr) {
                                auto h = std::move(*handler_ptr);
                                h->invoke(true);  // true = 正常完成
                            }
                        }
                    } else {
                        // 加入等待队列
                        auto wrapper = [completed, timer, handler_ptr]() mutable {
                            if (!completed->exchange(true, std::memory_order_relaxed)) {
                                timer->cancel();
                                if (*handler_ptr) {
                                    auto h = std::move(*handler_ptr);
                                    h->invoke(true);  // true = 正常完成
                                }
                            }
                        };
                        waiters_.push_back(
                            std::make_unique<detail::void_handler_impl<decltype(wrapper)>>(std::move(wrapper))
                        );
                    }
                });
            },
            token
        );
    }

    /**
     * @brief 获取当前计数（同步，轻量级）
     * 
     * 用途：
     * - 简单的状态检查（不需要严格的序列化保证）
     * - 日志输出、调试信息
     * - 非协程上下文中的快速查询
     * 
     * 注意事项：
     * - 返回值是时间点快照，可能立即过时
     * - 线程安全（atomic 读取）
     * - 如果需要基于 count 做决策，考虑使用 wait() 而非轮询 count
     * 
     * 示例：
     * @code
     * // ✅ 好：用于日志/调试
     * std::cout << "剩余任务: " << wg->count() << "\n";
     * 
     * // ⚠️ 谨慎：用于条件判断（存在竞态）
     * if (wg->count() == 0) {
     *     // count 可能在检查后立即变化
     *     do_something();
     * }
     * 
     * // ✅ 更好：使用 wait() 而非轮询
     * co_await wg->wait(use_awaitable);
     * do_something();  // 保证 count=0
     * @endcode
     */
    int64_t count() const noexcept {
        return count_.load(std::memory_order_acquire);
    }
    
    /**
     * @brief 异步获取当前计数
     * 
     * 用途：
     * - 需要与其他 strand 操作序列化时使用
     * - 协程上下文中想要明确异步语义
     * 
     * 大多数情况下，同步的 count() 已经足够。
     * 此方法主要用于特殊场景：需要确保 count 查询
     * 与其他异步操作的顺序关系。
     * 
     * 示例：
     * @code
     * // 确保 count 查询在某个异步操作之后
     * co_await some_async_op(use_awaitable);
     * auto c = co_await wg->async_count(use_awaitable);
     * // 保证看到 some_async_op 完成后的 count
     * @endcode
     */
    template<typename CompletionToken = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>>
    auto async_count(CompletionToken&& token = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>{}) {
        return asio::async_initiate<CompletionToken, void(int64_t)>(
            [this, self = shared_from_this()](auto handler) {
                asio::post(strand_, [this, self, handler = std::move(handler)]() mutable {
                    std::move(handler)(count_.load(std::memory_order_acquire));
                });
            },
            token
        );
    }

    executor_type get_executor() const noexcept {
        return strand_.get_inner_executor();
    }

private:
    /**
     * @brief 唤醒所有等待者（内部方法，必须在 strand 上调用）
     */
    void notify_all_waiters() {
        while (!waiters_.empty()) {
            auto handler = std::move(waiters_.front());
            waiters_.pop_front();
            handler->invoke();
        }
    }
};

} // namespace acore

