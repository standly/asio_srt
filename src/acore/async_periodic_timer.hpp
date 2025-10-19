//
// Async Periodic Timer - 周期定时器
//
#pragma once

#include <asio.hpp>
#include <chrono>
#include <memory>
#include <atomic>

namespace acore {

/**
 * @brief 周期性定时器
 * 
 * 特性：
 * 1. 自动重置的定时器（无需手动 expires_after）
 * 2. 支持暂停/恢复
 * 3. 支持动态修改周期
 * 4. 线程安全
 * 
 * 适用场景：
 * - 心跳包发送
 * - 定期统计上报
 * - 轮询操作
 * - 健康检查
 * 
 * 使用示例：
 * @code
 * auto timer = std::make_shared<async_periodic_timer>(io_ctx, 5s);
 * 
 * while (true) {
 *     co_await timer->async_wait(use_awaitable);
 *     send_heartbeat();
 * }
 * 
 * // 或使用 async_next（更语义化）
 * while (true) {
 *     co_await timer->async_next(use_awaitable);
 *     send_heartbeat();
 * }
 * @endcode
 */
class async_periodic_timer : public std::enable_shared_from_this<async_periodic_timer> {
private:
    using executor_type = asio::any_io_executor;
    using duration_type = std::chrono::nanoseconds;
    
    asio::strand<executor_type> strand_;
    std::unique_ptr<asio::steady_timer> timer_;
    duration_type period_;
    std::atomic<bool> running_{true};
    std::atomic<bool> paused_{false};
    
public:
    // 禁止拷贝和移动
    async_periodic_timer(const async_periodic_timer&) = delete;
    async_periodic_timer& operator=(const async_periodic_timer&) = delete;
    async_periodic_timer(async_periodic_timer&&) = delete;
    async_periodic_timer& operator=(async_periodic_timer&&) = delete;
    
    /**
     * @brief 构造函数
     * @param io_ctx ASIO io_context
     * @param period 定时器周期
     */
    template<typename Rep, typename Period>
    explicit async_periodic_timer(
        asio::io_context& io_ctx,
        std::chrono::duration<Rep, Period> period)
        : strand_(asio::make_strand(io_ctx.get_executor()))
        , timer_(std::make_unique<asio::steady_timer>(strand_))
        , period_(std::chrono::duration_cast<duration_type>(period))
    {}
    
    /**
     * @brief 构造函数（使用 executor）
     */
    template<typename Rep, typename Period>
    explicit async_periodic_timer(
        executor_type ex,
        std::chrono::duration<Rep, Period> period)
        : strand_(asio::make_strand(ex))
        , timer_(std::make_unique<asio::steady_timer>(strand_))
        , period_(std::chrono::duration_cast<duration_type>(period))
    {}
    
    /**
     * @brief 等待下一个周期（推荐接口）
     * 
     * 自动重置定时器到下一个周期
     * 
     * 用法：
     * @code
     * while (true) {
     *     co_await timer->async_next(use_awaitable);
     *     do_periodic_work();
     * }
     * @endcode
     */
    template<typename CompletionToken = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>>
    auto async_next(CompletionToken&& token = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>{}) {
        return async_wait(std::forward<CompletionToken>(token));
    }
    
    /**
     * @brief 等待下一个周期
     * 
     * 用法：
     * @code
     * while (true) {
     *     co_await timer->async_wait(use_awaitable);
     *     do_periodic_work();
     * }
     * @endcode
     */
    template<typename CompletionToken = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>>
    auto async_wait(CompletionToken&& token = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>{}) {
        return asio::async_initiate<CompletionToken, void()>(
            [self = shared_from_this()](auto handler) {
                asio::post(self->strand_, [self, handler = std::move(handler)]() mutable {
                    if (!self->running_ || self->paused_.load(std::memory_order_acquire)) {
                        // 停止或暂停时不执行（静默忽略）
                        return;
                    }
                    
                    // 设置定时器
                    self->timer_->expires_after(self->period_);
                    
                    self->timer_->async_wait(
                        [self, handler = std::move(handler)](const std::error_code& ec) mutable {
                            if (!ec && self->running_ && !self->paused_.load(std::memory_order_acquire)) {
                                handler();
                            }
                        }
                    );
                });
            },
            token
        );
    }
    
    /**
     * @brief 停止定时器
     * 
     * 取消所有等待操作
     */
    void stop() {
        running_.store(false, std::memory_order_release);
        asio::post(strand_, [self = shared_from_this()]() {
            self->timer_->cancel();
        });
    }
    
    /**
     * @brief 暂停定时器
     * 
     * 后续的 async_wait 调用会立即返回错误
     */
    void pause() {
        paused_.store(true, std::memory_order_release);
        asio::post(strand_, [self = shared_from_this()]() {
            self->timer_->cancel();
        });
    }
    
    /**
     * @brief 恢复定时器
     */
    void resume() {
        paused_.store(false, std::memory_order_release);
    }
    
    /**
     * @brief 重启定时器（重置状态）
     */
    void restart() {
        running_.store(true, std::memory_order_release);
        paused_.store(false, std::memory_order_release);
    }
    
    /**
     * @brief 修改定时器周期
     * 
     * 注意：修改会在下一次 async_wait 调用时生效
     */
    template<typename Rep, typename Period>
    void set_period(std::chrono::duration<Rep, Period> new_period) {
        auto ns = std::chrono::duration_cast<duration_type>(new_period);
        asio::post(strand_, [self = shared_from_this(), ns]() {
            self->period_ = ns;
        });
    }
    
    /**
     * @brief 获取当前周期
     */
    template<typename Duration = std::chrono::milliseconds>
    Duration get_period() const {
        return std::chrono::duration_cast<Duration>(period_);
    }
    
    /**
     * @brief 检查是否正在运行
     */
    bool is_running() const noexcept {
        return running_.load(std::memory_order_acquire);
    }
    
    /**
     * @brief 检查是否暂停
     */
    bool is_paused() const noexcept {
        return paused_.load(std::memory_order_acquire);
    }
    
    executor_type get_executor() const noexcept {
        return strand_.get_inner_executor();
    }
};

/**
 * @brief 一次性异步定时器（辅助工具）
 * 
 * 简化版的定时器，只等待一次
 */
class async_timer {
private:
    asio::steady_timer timer_;
    
public:
    explicit async_timer(asio::io_context& io_ctx)
        : timer_(io_ctx)
    {}
    
    explicit async_timer(asio::any_io_executor ex)
        : timer_(ex)
    {}
    
    /**
     * @brief 等待指定时长
     */
    template<typename Rep, typename Period, typename CompletionToken>
    auto async_wait_for(
        std::chrono::duration<Rep, Period> duration,
        CompletionToken&& token)
    {
        timer_.expires_after(duration);
        return timer_.async_wait(std::forward<CompletionToken>(token));
    }
    
    /**
     * @brief 等待到指定时间点
     */
    template<typename Clock, typename Duration, typename CompletionToken>
    auto async_wait_until(
        std::chrono::time_point<Clock, Duration> time_point,
        CompletionToken&& token)
    {
        timer_.expires_at(time_point);
        return timer_.async_wait(std::forward<CompletionToken>(token));
    }
    
    /**
     * @brief 取消等待
     */
    void cancel() {
        timer_.cancel();
    }
};

} // namespace acore

