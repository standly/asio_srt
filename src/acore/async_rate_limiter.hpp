//
// Async Rate Limiter - 异步速率限制器（令牌桶算法）
//
#pragma once

#include <asio.hpp>
#include <deque>
#include <memory>
#include <chrono>
#include "handler_traits.hpp"

namespace acore {

/**
 * @brief 异步速率限制器（基于令牌桶算法）
 * 
 * 特性：
 * 1. 令牌桶算法实现流量控制
 * 2. 支持突发流量（burst）
 * 3. 支持可变令牌消耗（按大小限流）
 * 4. 线程安全（使用 strand）
 * 
 * 工作原理：
 * - 令牌以恒定速率添加到桶中
 * - 桶有最大容量（支持突发）
 * - 操作需要消耗令牌
 * - 令牌不足时等待
 * 
 * 适用场景：
 * - 带宽限制
 * - API 调用频率限制
 * - 连接速率限制
 * - QoS 流量整形
 * 
 * 使用示例：
 * @code
 * // 限制：每秒 100 个请求，允许突发 200 个
 * auto limiter = std::make_shared<async_rate_limiter>(
 *     io_ctx, 100, 1s, 200
 * );
 * 
 * // 获取令牌（消耗 1 个）
 * co_await limiter->async_acquire(use_awaitable);
 * send_request();
 * 
 * // 按大小消耗令牌（如带宽限制）
 * co_await limiter->async_acquire(packet_size, use_awaitable);
 * send_packet(packet);
 * @endcode
 */
class async_rate_limiter : public std::enable_shared_from_this<async_rate_limiter> {
private:
    using executor_type = asio::any_io_executor;
    using clock_type = std::chrono::steady_clock;
    using time_point = clock_type::time_point;
    using duration_type = std::chrono::nanoseconds;
    
    struct waiter {
        size_t tokens_needed;
        std::unique_ptr<detail::void_handler_base> handler;
    };
    
    asio::strand<executor_type> strand_;
    std::unique_ptr<asio::steady_timer> refill_timer_;
    
    size_t rate_;                    // 每个周期的令牌数
    duration_type period_;           // 周期时长
    size_t capacity_;                // 桶容量（最大令牌数）
    double tokens_;                  // 当前令牌数（使用 double 支持精确计算）
    time_point last_refill_;         // 上次填充时间
    std::deque<waiter> waiters_;     // 等待队列
    bool running_{true};

public:
    // 禁止拷贝和移动
    async_rate_limiter(const async_rate_limiter&) = delete;
    async_rate_limiter& operator=(const async_rate_limiter&) = delete;
    async_rate_limiter(async_rate_limiter&&) = delete;
    async_rate_limiter& operator=(async_rate_limiter&&) = delete;
    
    /**
     * @brief 构造函数
     * @param io_ctx ASIO io_context
     * @param rate 每个周期的令牌数（速率）
     * @param period 周期时长
     * @param capacity 桶容量（默认等于 rate，即不支持突发）
     */
    template<typename Rep, typename Period>
    explicit async_rate_limiter(
        asio::io_context& io_ctx,
        size_t rate,
        std::chrono::duration<Rep, Period> period,
        size_t capacity = 0)
        : strand_(asio::make_strand(io_ctx.get_executor()))
        , refill_timer_(std::make_unique<asio::steady_timer>(strand_))
        , rate_(rate)
        , period_(std::chrono::duration_cast<duration_type>(period))
        , capacity_(capacity == 0 ? rate : capacity)
        , tokens_(static_cast<double>(capacity == 0 ? rate : capacity))
        , last_refill_(clock_type::now())
    {
        if (rate == 0) {
            throw std::invalid_argument("rate must be greater than 0");
        }
        if (this->capacity_ < rate) {
            throw std::invalid_argument("capacity must be >= rate");
        }
    }
    
    /**
     * @brief 构造函数（使用 executor）
     */
    template<typename Rep, typename Period>
    explicit async_rate_limiter(
        executor_type ex,
        size_t rate,
        std::chrono::duration<Rep, Period> period,
        size_t capacity = 0)
        : strand_(asio::make_strand(ex))
        , refill_timer_(std::make_unique<asio::steady_timer>(strand_))
        , rate_(rate)
        , period_(std::chrono::duration_cast<duration_type>(period))
        , capacity_(capacity == 0 ? rate : capacity)
        , tokens_(static_cast<double>(capacity == 0 ? rate : capacity))
        , last_refill_(clock_type::now())
    {
        if (rate == 0) {
            throw std::invalid_argument("rate must be greater than 0");
        }
        if (this->capacity_ < rate) {
            throw std::invalid_argument("capacity must be >= rate");
        }
    }
    
    /**
     * @brief 异步获取令牌（默认 1 个）
     */
    template<typename CompletionToken = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>>
    auto async_acquire(CompletionToken&& token = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>{})
    {
        return async_acquire(1, std::forward<CompletionToken>(token));
    }
    
    /**
     * @brief 异步获取令牌（指定数量）
     * 
     * 如果令牌足够，立即返回
     * 否则，等待令牌补充
     * 
     * @param tokens 需要的令牌数
     */
    template<typename CompletionToken = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>>
    auto async_acquire(
        size_t tokens,
        CompletionToken&& token = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>{})
    {
        return asio::async_initiate<CompletionToken, void()>(
            [self = shared_from_this(), tokens](auto handler) {
                asio::post(self->strand_, [self, tokens, handler = std::move(handler)]() mutable {
                    if (!self->running_) {
                        handler();
                        return;
                    }
                    
                    // 先填充令牌
                    self->refill_tokens();
                    
                    // 检查令牌是否足够
                    if (self->tokens_ >= static_cast<double>(tokens)) {
                        // 令牌足够，立即消耗并返回
                        self->tokens_ -= static_cast<double>(tokens);
                        handler();
                    } else {
                        // 令牌不足，加入等待队列
                        self->waiters_.push_back({
                            tokens,
                            std::make_unique<detail::void_handler_impl<decltype(handler)>>(std::move(handler))
                        });
                        
                        // 启动定时器定期填充
                        self->schedule_refill();
                    }
                });
            },
            token
        );
    }
    
    /**
     * @brief 尝试获取令牌（非阻塞，默认 1 个）
     */
    template<typename CompletionToken = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>>
    auto async_try_acquire(CompletionToken&& token = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>{})
    {
        return async_try_acquire(1, std::forward<CompletionToken>(token));
    }
    
    /**
     * @brief 尝试获取令牌（非阻塞，指定数量）
     * 
     * @param tokens 需要的令牌数
     * @return true - 成功获取，false - 令牌不足
     */
    template<typename CompletionToken = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>>
    auto async_try_acquire(
        size_t tokens,
        CompletionToken&& token = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>{})
    {
        return asio::async_initiate<CompletionToken, void(bool)>(
            [self = shared_from_this(), tokens](auto handler) {
                asio::post(self->strand_, [self, tokens, handler = std::move(handler)]() mutable {
                    if (!self->running_) {
                        handler(false);
                        return;
                    }
                    
                    // 先填充令牌
                    self->refill_tokens();
                    
                    // 检查令牌是否足够
                    if (self->tokens_ >= static_cast<double>(tokens)) {
                        // 令牌足够，消耗并返回 true
                        self->tokens_ -= static_cast<double>(tokens);
                        handler(true);
                    } else {
                        // 令牌不足，返回 false
                        handler(false);
                    }
                });
            },
            token
        );
    }
    
    /**
     * @brief 停止限流器
     * 
     * 立即完成所有等待的操作
     */
    void stop() {
        asio::post(strand_, [self = shared_from_this()]() {
            self->running_ = false;
            self->refill_timer_->cancel();
            
            // 完成所有等待者
            while (!self->waiters_.empty()) {
                auto w = std::move(self->waiters_.front());
                self->waiters_.pop_front();
                w.handler->invoke();
            }
        });
    }
    
    /**
     * @brief 重置限流器
     * 
     * 将令牌数重置为容量
     */
    void reset() {
        asio::post(strand_, [self = shared_from_this()]() {
            self->tokens_ = static_cast<double>(self->capacity_);
            self->last_refill_ = clock_type::now();
            self->running_ = true;
            
            // 尝试满足等待者
            self->process_waiters();
        });
    }
    
    /**
     * @brief 获取当前可用令牌数
     */
    template<typename CompletionToken = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>>
    auto async_available_tokens(CompletionToken&& token = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>{}) {
        return asio::async_initiate<CompletionToken, void(size_t)>(
            [self = shared_from_this()](auto handler) {
                asio::post(self->strand_, [self, handler = std::move(handler)]() mutable {
                    self->refill_tokens();
                    handler(static_cast<size_t>(self->tokens_));
                });
            },
            token
        );
    }
    
    /**
     * @brief 获取等待者数量
     */
    template<typename CompletionToken = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>>
    auto async_waiting_count(CompletionToken&& token = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>{}) {
        return asio::async_initiate<CompletionToken, void(size_t)>(
            [self = shared_from_this()](auto handler) {
                asio::post(self->strand_, [self, handler = std::move(handler)]() mutable {
                    handler(self->waiters_.size());
                });
            },
            token
        );
    }
    
    /**
     * @brief 修改速率
     */
    void set_rate(size_t new_rate) {
        asio::post(strand_, [self = shared_from_this(), new_rate]() {
            if (new_rate > 0) {
                self->rate_ = new_rate;
            }
        });
    }
    
    /**
     * @brief 获取当前速率
     */
    size_t get_rate() const {
        return rate_;
    }
    
    /**
     * @brief 获取桶容量
     */
    size_t get_capacity() const {
        return capacity_;
    }
    
    executor_type get_executor() const noexcept {
        return strand_.get_inner_executor();
    }

private:
    /**
     * @brief 填充令牌（基于时间流逝）
     */
    void refill_tokens() {
        auto now = clock_type::now();
        auto elapsed = now - last_refill_;
        
        if (elapsed.count() <= 0) {
            return;
        }
        
        // 计算应该添加的令牌数
        // tokens_to_add = (elapsed / period) * rate
        double elapsed_periods = static_cast<double>(elapsed.count()) / 
                                static_cast<double>(period_.count());
        double tokens_to_add = elapsed_periods * static_cast<double>(rate_);
        
        // 添加令牌，不超过容量
        tokens_ = std::min(tokens_ + tokens_to_add, static_cast<double>(capacity_));
        
        last_refill_ = now;
    }
    
    /**
     * @brief 处理等待队列
     */
    void process_waiters() {
        while (!waiters_.empty()) {
            auto& w = waiters_.front();
            
            if (tokens_ >= static_cast<double>(w.tokens_needed)) {
                // 令牌足够，满足这个等待者
                tokens_ -= static_cast<double>(w.tokens_needed);
                
                auto handler = std::move(w.handler);
                waiters_.pop_front();
                handler->invoke();
            } else {
                // 令牌不足，停止处理
                break;
            }
        }
    }
    
    /**
     * @brief 调度定期填充
     */
    void schedule_refill() {
        if (waiters_.empty() || !running_) {
            return;
        }
        
        // 计算下次填充时间
        // 为了满足第一个等待者，需要多少时间？
        auto& first_waiter = waiters_.front();
        double tokens_needed = static_cast<double>(first_waiter.tokens_needed) - tokens_;
        
        if (tokens_needed <= 0) {
            // 已经有足够的令牌，立即处理
            process_waiters();
            return;
        }
        
        // 计算需要等待的时间
        // time = (tokens_needed / rate) * period
        double periods_needed = tokens_needed / static_cast<double>(rate_);
        auto wait_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::duration<double, std::nano>(
                periods_needed * static_cast<double>(period_.count())
            )
        );
        
        // 设置定时器
        refill_timer_->expires_after(wait_duration);
        refill_timer_->async_wait(
            [self = shared_from_this()](const std::error_code& ec) {
                if (ec) return;
                
                asio::post(self->strand_, [self]() {
                    if (!self->running_) return;
                    
                    // 填充令牌
                    self->refill_tokens();
                    
                    // 处理等待者
                    self->process_waiters();
                    
                    // 如果还有等待者，继续调度
                    if (!self->waiters_.empty()) {
                        self->schedule_refill();
                    }
                });
            }
        );
    }
};

} // namespace acore

