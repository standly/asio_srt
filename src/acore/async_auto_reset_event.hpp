//
// Async Auto-Reset Event - 自动重置事件
//
#pragma once

#include <asio.hpp>
#include <deque>
#include <memory>
#include "handler_traits.hpp"

namespace acore {

/**
 * @brief 自动重置事件
 * 
 * 特性：
 * 1. notify() 只唤醒一个等待者（而非全部）
 * 2. 唤醒后自动重置（无需手动 reset）
 * 3. 类似 Win32 的 Auto-Reset Event
 * 4. 线程安全（使用 strand）
 * 
 * 适用场景：
 * - 单次通知（一对一）
 * - 任务分发
 * - 请求-响应模式
 * - 事件队列
 * 
 * vs async_event（手动重置）：
 * - async_event: notify_all() 唤醒所有等待者（广播）
 * - async_auto_reset_event: notify() 只唤醒一个（单播）
 * 
 * 使用示例：
 * @code
 * auto event = std::make_shared<async_auto_reset_event>(io_ctx);
 * 
 * // 多个 worker 等待任务
 * for (int i = 0; i < 5; ++i) {
 *     co_spawn(io_ctx, [event, i]() -> awaitable<void> {
 *         while (true) {
 *             co_await event->wait(use_awaitable);  // 等待任务
 *             process_task(i);  // 只有一个 worker 被唤醒
 *         }
 *     }, detached);
 * }
 * 
 * // 分发任务
 * event->notify();  // 唤醒一个 worker
 * @endcode
 */
class async_auto_reset_event : public std::enable_shared_from_this<async_auto_reset_event> {
private:
    using executor_type = asio::any_io_executor;
    
    asio::strand<executor_type> strand_;
    std::deque<std::unique_ptr<detail::void_handler_base>> waiters_;  // 等待队列
    size_t signal_count_{0};  // 信号计数（仅在 strand 内访问）

public:
    // 禁止拷贝和移动
    async_auto_reset_event(const async_auto_reset_event&) = delete;
    async_auto_reset_event& operator=(const async_auto_reset_event&) = delete;
    async_auto_reset_event(async_auto_reset_event&&) = delete;
    async_auto_reset_event& operator=(async_auto_reset_event&&) = delete;
    
    /**
     * @brief 构造函数（创建内部 strand）- io_context 版本
     * @param io_ctx ASIO io_context
     * @param initially_set 初始状态（默认未设置）
     */
    explicit async_auto_reset_event(
        asio::io_context& io_ctx,
        bool initially_set = false)
        : strand_(asio::make_strand(io_ctx.get_executor()))
        , signal_count_(initially_set ? 1 : 0)
    {}
    
    /**
     * @brief 构造函数（创建内部 strand）- executor 版本
     * @param ex executor
     * @param initially_set 初始状态（默认未设置）
     */
    explicit async_auto_reset_event(
        executor_type ex,
        bool initially_set = false)
        : strand_(asio::make_strand(ex))
        , signal_count_(initially_set ? 1 : 0)
    {}
    
    /**
     * @brief 构造函数（使用外部 strand）
     * 
     * 使用场景：当 event 与其他组件共享 strand 时
     * 
     * @param strand 外部提供的 strand
     * @param initially_set 初始状态（默认未设置）
     */
    explicit async_auto_reset_event(
        asio::strand<executor_type> strand,
        bool initially_set = false)
        : strand_(strand)
        , signal_count_(initially_set ? 1 : 0)
    {}
    
    /**
     * @brief 等待事件
     * 
     * 如果有信号，立即消耗一个信号并返回
     * 否则，加入等待队列
     * 
     * 用法：
     * @code
     * co_await event->wait(use_awaitable);
     * // 事件已触发（自动重置）
     * @endcode
     */
    template<typename CompletionToken = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>>
    auto wait(CompletionToken&& token = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>{}) {
        return asio::async_initiate<CompletionToken, void()>(
            [self = shared_from_this()](auto handler) {
                asio::post(self->strand_, [self, handler = std::move(handler)]() mutable {
                    if (self->signal_count_ > 0) {
                        // 有信号，消耗一个并立即返回
                        self->signal_count_--;
                        std::move(handler)();
                    } else {
                        // 无信号，加入等待队列
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
     * @brief 触发事件（唤醒一个等待者）
     * 
     * 如果有等待者，唤醒队首的一个
     * 否则，增加信号计数（供下次 wait 使用）
     */
    void notify() {
        asio::post(strand_, [self = shared_from_this()]() {
            if (!self->waiters_.empty()) {
                // 有等待者，唤醒第一个
                auto handler = std::move(self->waiters_.front());
                self->waiters_.pop_front();
                handler->invoke();
                // 不增加 signal_count_，因为已经唤醒了等待者
            } else {
                // 无等待者，增加信号计数
                self->signal_count_++;
            }
        });
    }
    
    /**
     * @brief 批量通知（唤醒 n 个等待者）
     * 
     * @param count 要唤醒的等待者数量
     */
    void notify(size_t count) {
        if (count == 0) return;
        
        asio::post(strand_, [self = shared_from_this(), count]() {
            for (size_t i = 0; i < count; ++i) {
                if (!self->waiters_.empty()) {
                    // 有等待者，唤醒一个
                    auto handler = std::move(self->waiters_.front());
                    self->waiters_.pop_front();
                    handler->invoke();
                } else {
                    // 无等待者，增加信号计数
                    self->signal_count_++;
                }
            }
        });
    }
    
    /**
     * @brief 尝试等待（非阻塞）
     * 
     * @return true - 成功获取信号，false - 无信号
     */
    template<typename CompletionToken = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>>
    auto try_wait(CompletionToken&& token = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>{}) {
        return asio::async_initiate<CompletionToken, void(bool)>(
            [self = shared_from_this()](auto handler) {
                asio::post(self->strand_, [self, handler = std::move(handler)]() mutable {
                    bool success = false;
                    if (self->signal_count_ > 0) {
                        self->signal_count_--;
                        success = true;
                    }
                    std::move(handler)(success);
                });
            },
            token
        );
    }
    
    /**
     * @brief 重置事件（清空所有信号）
     * 
     * 注意：不会影响等待队列中的等待者
     */
    void reset() {
        asio::post(strand_, [self = shared_from_this()]() {
            self->signal_count_ = 0;
        });
    }
    
    /**
     * @brief 取消所有等待者
     * 
     * 清空等待队列，所有等待者会立即返回
     */
    void cancel_all() {
        asio::post(strand_, [self = shared_from_this()]() {
            while (!self->waiters_.empty()) {
                auto handler = std::move(self->waiters_.front());
                self->waiters_.pop_front();
                handler->invoke();
            }
        });
    }
    
    /**
     * @brief 获取信号计数
     */
    template<typename CompletionToken = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>>
    auto async_signal_count(CompletionToken&& token = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>{}) {
        return asio::async_initiate<CompletionToken, void(size_t)>(
            [self = shared_from_this()](auto handler) {
                asio::post(self->strand_, [self, handler = std::move(handler)]() mutable {
                    std::move(handler)(self->signal_count_);
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

