//
// Async Latch - 异步一次性计数器
//
#pragma once

#include <asio.hpp>
#include <deque>
#include <memory>
#include <atomic>
#include "handler_traits.hpp"

namespace acore {

/**
 * @brief 异步一次性计数器（类似 std::latch）
 * 
 * 特性：
 * 1. 只能倒计数（不支持增加）
 * 2. 到达 0 后触发所有等待者
 * 3. 一次性使用（不可重置）
 * 4. 线程安全
 * 
 * vs async_waitgroup：
 * - async_waitgroup: 支持 add()，可以动态增加计数
 * - async_latch: 只能倒计数，更简单、更轻量
 * 
 * 适用场景：
 * - 一次性启动屏障（等待初始化完成）
 * - 等待固定数量的任务完成
 * - 关闭协调（等待所有清理完成）
 * 
 * 使用示例：
 * @code
 * // 等待 3 个任务完成
 * auto latch = std::make_shared<async_latch>(io_ctx, 3);
 * 
 * // 启动 3 个任务
 * for (int i = 0; i < 3; ++i) {
 *     co_spawn(io_ctx, [latch, i]() -> awaitable<void> {
 *         do_work(i);
 *         latch->count_down();  // 完成后倒计数
 *     }, detached);
 * }
 * 
 * // 等待所有任务完成
 * co_await latch->wait(use_awaitable);
 * std::cout << "All tasks completed!\n";
 * @endcode
 */
class async_latch : public std::enable_shared_from_this<async_latch> {
private:
    using executor_type = asio::any_io_executor;
    
    asio::strand<executor_type> strand_;
    std::atomic<int64_t> count_;  // 计数器（原子操作）
    std::deque<std::unique_ptr<detail::void_handler_base>> waiters_;  // 等待队列（仅在 strand 内访问）
    std::atomic<bool> triggered_{false};  // 是否已触发（原子操作）
    std::atomic<uint64_t> error_count_{0};  // 错误计数（count_down 导致下溢的次数）

public:
    // 禁止拷贝和移动
    async_latch(const async_latch&) = delete;
    async_latch& operator=(const async_latch&) = delete;
    async_latch(async_latch&&) = delete;
    async_latch& operator=(async_latch&&) = delete;
    
    /**
     * @brief 构造函数
     * @param io_ctx ASIO io_context
     * @param initial_count 初始计数（必须 >= 0）
     */
    explicit async_latch(asio::io_context& io_ctx, int64_t initial_count)
        : strand_(asio::make_strand(io_ctx.get_executor()))
        , count_(initial_count)
    {
        if (initial_count < 0) {
            throw std::invalid_argument("initial_count must be >= 0");
        }
        
        // 如果初始计数为 0，立即标记为已触发
        if (initial_count == 0) {
            triggered_.store(true, std::memory_order_release);
        }
    }
    
    /**
     * @brief 构造函数（使用 executor）
     */
    explicit async_latch(executor_type ex, int64_t initial_count)
        : strand_(asio::make_strand(ex))
        , count_(initial_count)
    {
        if (initial_count < 0) {
            throw std::invalid_argument("initial_count must be >= 0");
        }
        
        if (initial_count == 0) {
            triggered_.store(true, std::memory_order_release);
        }
    }
    
    /**
     * @brief 倒计数
     * 
     * 减少计数器。当计数到达 0 时，唤醒所有等待者
     * 
     * @param n 减少的数量（默认 1）
     * 
     * 注意：如果计数变为负数，会断言失败（debug）或设为 0（release）
     */
    void count_down(int64_t n = 1) {
        if (n <= 0) return;
        
        // 原子减少计数
        int64_t old_count = count_.fetch_sub(n, std::memory_order_acq_rel);
        int64_t new_count = old_count - n;
        
        if (new_count < 0) {
            // 错误：计数变为负数（count_down 调用次数过多）
            count_.store(0, std::memory_order_release);
            error_count_.fetch_add(1, std::memory_order_relaxed);  // 记录错误
            // 注意：不使用 assert，因为在 Release 模式下会被编译掉
            // 用户可以通过 get_error_count() 检查是否有错误
            new_count = 0;
        }
        
        // 如果计数到达 0，触发所有等待者
        if (new_count == 0 && !triggered_.exchange(true, std::memory_order_acq_rel)) {
            asio::post(strand_, [self = shared_from_this()]() {
                // 唤醒所有等待者
                while (!self->waiters_.empty()) {
                    auto handler = std::move(self->waiters_.front());
                    self->waiters_.pop_front();
                    handler->invoke();
                }
            });
        }
    }
    
    /**
     * @brief 到达并等待
     * 
     * 等效于：count_down(); co_await wait();
     * 但更高效（单次异步操作）
     */
    template<typename CompletionToken = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>>
    auto arrive_and_wait(CompletionToken&& token = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>{})
    {
        return arrive_and_wait(1, std::forward<CompletionToken>(token));
    }
    
    /**
     * @brief 到达并等待（指定倒计数）
     */
    template<typename CompletionToken = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>>
    auto arrive_and_wait(
        int64_t n,
        CompletionToken&& token = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>{})
    {
        return asio::async_initiate<CompletionToken, void()>(
            [self = shared_from_this(), n](auto handler) {
                // 先倒计数
                if (n > 0) {
                    int64_t old_count = self->count_.fetch_sub(n, std::memory_order_acq_rel);
                    int64_t new_count = old_count - n;
                    
                    if (new_count < 0) {
                        self->count_.store(0, std::memory_order_release);
                        self->error_count_.fetch_add(1, std::memory_order_relaxed);  // 记录错误
                        new_count = 0;
                    }
                    
                    // 如果到达 0 且是首次触发
                    if (new_count == 0 && !self->triggered_.exchange(true, std::memory_order_acq_rel)) {
                        // 当前调用者就是最后一个
                        asio::post(self->strand_, [self, handler = std::move(handler)]() mutable {
                            // 唤醒所有之前的等待者
                            while (!self->waiters_.empty()) {
                                auto h = std::move(self->waiters_.front());
                                self->waiters_.pop_front();
                                h->invoke();
                            }
                            
                            // 当前调用者也完成
                            std::move(handler)();
                        });
                        return;
                    }
                }
                
                // 否则，正常等待
                asio::post(self->strand_, [self, handler = std::move(handler)]() mutable {
                    if (self->triggered_.load(std::memory_order_acquire)) {
                        // 已经触发，立即完成
                        std::move(handler)();
                    } else {
                        // 加入等待队列
                        self->waiters_.push_back(
                            std::make_unique<detail::void_handler_impl<decltype(handler)>>(std::move(handler))
                        );
                    }
                });
            },
            token
        );
    }
    
    /**
     * @brief 等待计数到达 0
     * 
     * 如果已经到达 0，立即返回
     * 否则，等待直到计数到达 0
     */
    template<typename CompletionToken = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>>
    auto wait(CompletionToken&& token = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>{}) {
        return asio::async_initiate<CompletionToken, void()>(
            [self = shared_from_this()](auto handler) {
                asio::post(self->strand_, [self, handler = std::move(handler)]() mutable {
                    if (self->triggered_.load(std::memory_order_acquire)) {
                        // 已经触发，立即完成
                        std::move(handler)();
                    } else {
                        // 加入等待队列
                        self->waiters_.push_back(
                            std::make_unique<detail::void_handler_impl<decltype(handler)>>(std::move(handler))
                        );
                    }
                });
            },
            token
        );
    }
    
    /**
     * @brief 尝试等待（非阻塞）
     * 
     * @return true - 已经到达 0，false - 尚未到达
     */
    bool try_wait() const noexcept {
        return triggered_.load(std::memory_order_acquire);
    }
    
    /**
     * @brief 获取当前计数
     * 
     * 注意：返回值是时间点快照，可能立即过时
     */
    int64_t count() const noexcept {
        int64_t c = count_.load(std::memory_order_acquire);
        return c < 0 ? 0 : c;
    }
    
    /**
     * @brief 检查是否已触发
     */
    bool is_ready() const noexcept {
        return triggered_.load(std::memory_order_acquire);
    }
    
    /**
     * @brief 获取错误计数
     * 
     * 返回 count_down() 导致计数下溢的次数
     * 如果返回非零值，表示 count_down() 被调用的次数超过了初始计数
     * 
     * @return 错误次数（0 表示无错误）
     */
    uint64_t get_error_count() const noexcept {
        return error_count_.load(std::memory_order_relaxed);
    }
    
    /**
     * @brief 获取等待者数量
     */
    template<typename CompletionToken = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>>
    auto async_waiting_count(CompletionToken&& token = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>{}) {
        return asio::async_initiate<CompletionToken, void(size_t)>(
            [self = shared_from_this()](auto handler) {
                asio::post(self->strand_, [self, handler = std::move(handler)]() mutable {
                    std::move(handler)(self->waiters_.size());
                });
            },
            token
        );
    }
    
    executor_type get_executor() const noexcept {
        return strand_.get_inner_executor();
    }
};

} // namespace acore

