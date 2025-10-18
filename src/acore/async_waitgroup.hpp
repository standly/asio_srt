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
 * 示例：
 * @code
 * auto wg = std::make_shared<async_waitgroup>(io_context);
 * 
 * // 启动3个异步任务
 * wg->add(3);
 * for (int i = 0; i < 3; ++i) {
 *     asio::co_spawn(io_context, [wg, i]() -> asio::awaitable<void> {
 *         // ... 做一些异步工作 ...
 *         wg->done();  // 完成
 *     }, asio::detached);
 * }
 * 
 * // 等待所有任务完成
 * co_await wg->wait(asio::use_awaitable);
 * std::cout << "所有任务完成！\n";
 * @endcode
 */
class async_waitgroup : public std::enable_shared_from_this<async_waitgroup> {
private:
    using executor_type = asio::any_io_executor;

    asio::strand<executor_type> strand_;
    std::deque<std::unique_ptr<detail::void_handler_base>> waiters_;  // 仅在 strand 内访问
    // count_ 在 strand 内访问，不需要 atomic（异步语义，所有更新都通过 strand 序列化）
    int64_t count_{0};  // 仅在 strand 内访问

public:
    // 禁止拷贝和移动（设计上通过 shared_ptr 使用）
    async_waitgroup(const async_waitgroup&) = delete;
    async_waitgroup& operator=(const async_waitgroup&) = delete;
    async_waitgroup(async_waitgroup&&) = delete;
    async_waitgroup& operator=(async_waitgroup&&) = delete;

    /**
     * @brief 构造函数
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
     * @brief 增加计数器（异步操作）
     * 
     * @param delta 增加的数量（可以为负数，但结果不能为负）
     * 
     * 语义：计数更新是异步的（通过 strand 序列化），这符合异步编程模式
     * 
     * 典型用法：
     * - 在启动异步任务前调用 add(1)
     * - 可以批量添加：add(10)
     * 
     * 性能建议：
     * - 每次调用 add() 都会 post 一个任务到 strand
     * - 如果要启动 N 个任务，使用 add(N) 比调用 N 次 add(1) 更高效
     * - 批量操作可以减少 post 开销和 lambda 对象创建
     * 
     * 错误处理：如果计数变为负数，会触发断言（debug）或恢复为0（release）
     */
    void add(int64_t delta = 1) {
        if (delta == 0) return;
        
        asio::post(strand_, [this, self = shared_from_this(), delta]() {
            count_ += delta;
            
            if (count_ < 0) {
                // 错误：计数变为负数（done()调用次数超过add()）
                assert(false && "async_waitgroup: negative counter (done() called too many times)");
                count_ = 0;
            }
            
            // 如果计数归零，唤醒所有等待者
            if (count_ == 0) {
                notify_all_waiters();
            }
        });
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
                    if (count_ == 0) {
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
                    if (count_ == 0) {
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
     * @brief 异步获取当前计数
     * 
     * 设计说明：
     * - 使用异步接口而非同步的 count() 方法
     * - 原因：同步方法返回的值可能在下一时刻就失效（竞态条件）
     * - 异步接口强制用户意识到这是一个时间点的快照，仅在回调时刻有效
     * - 这避免了给用户虚假的线程安全感
     */
    template<typename CompletionToken = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>>
    auto async_count(CompletionToken&& token = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>{}) {
        return asio::async_initiate<CompletionToken, void(int64_t)>(
            [this, self = shared_from_this()](auto handler) {
                asio::post(strand_, [this, self, handler = std::move(handler)]() mutable {
                    std::move(handler)(count_);
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

