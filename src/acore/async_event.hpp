//
// Async Event (Manual-Reset Event) - 参考 async_semaphore 重写
//
#pragma once

#include <asio.hpp>
#include <deque>
#include <memory>
#include <atomic>
#include <chrono>
#include "handler_traits.hpp"

namespace acore {

/**
 * @brief 异步事件（手动重置事件）
 * 
 * 特性：
 * 1. notify_all() 唤醒所有等待者（广播）
 * 2. 支持手动 reset()
 * 3. 线程安全（使用 strand）
 * 4. 支持超时等待
 * 
 * 适用场景：
 * - 事件广播（如：连接状态变化）
 * - 多个订阅者需要同时响应
 * - 状态同步
 */
class async_event {
private:
    using executor_type = asio::any_io_executor;

    asio::strand<executor_type> strand_;
    std::deque<std::unique_ptr<detail::void_handler_base>> waiters_;  // 仅在 strand 内访问
    bool is_set_{false};  // 仅在 strand 内访问，不需要 atomic

public:
    // 禁止拷贝和移动（设计上通过 shared_ptr 使用）
    async_event(const async_event&) = delete;
    async_event& operator=(const async_event&) = delete;
    async_event(async_event&&) = delete;
    async_event& operator=(async_event&&) = delete;

    /**
     * @brief 构造函数（创建内部 strand）
     * @param ex executor
     */
    explicit async_event(executor_type ex) 
        : strand_(asio::make_strand(ex)) 
    {}
    
    /**
     * @brief 构造函数（使用外部 strand）
     * 
     * 使用场景：当 event 与其他组件共享 strand 时
     * 
     * @param strand 外部提供的 strand
     */
    explicit async_event(asio::strand<executor_type> strand)
        : strand_(strand)
    {}

    /**
     * @brief 等待事件触发
     * 
     * 用法：co_await event.wait(use_awaitable);
     */
    template<typename CompletionToken = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>>
    auto wait(CompletionToken&& token = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>{}) {
        return asio::async_initiate<CompletionToken, void()>(
            [this](auto handler) {
                asio::post(strand_, [this, handler = std::move(handler)]() mutable {
                    if (is_set_) {
                        // 事件已触发，立即完成
                        std::move(handler)();
                    } else {
                        // 事件未触发，加入等待队列
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
     * 用法：bool triggered = co_await event.wait_for(5s, use_awaitable);
     * 
     * @return true - 事件在超时前触发
     *         false - 等待超时
     */
    template<typename Rep, typename Period, typename CompletionToken = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>>
    auto wait_for(
        std::chrono::duration<Rep, Period> timeout,
        CompletionToken&& token = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>{})
    {
        return asio::async_initiate<CompletionToken, void(bool)>(
            [this, timeout](auto handler) {
                // 完成标志：确保 handler 只被调用一次
                auto completed = std::make_shared<std::atomic<bool>>(false);
                auto timer = std::make_shared<asio::steady_timer>(strand_.get_inner_executor());
                
                // 将 handler 类型擦除并存储在 shared_ptr 中，两个路径共享
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
                            h->invoke(false);
                        }
                    }
                });
                
                // 事件等待
                asio::post(strand_, [this, completed, timer, handler_ptr]() mutable {
                    if (is_set_) {
                        // 事件已触发
                        if (!completed->exchange(true, std::memory_order_relaxed)) {
                            timer->cancel();
                            if (*handler_ptr) {
                                auto h = std::move(*handler_ptr);
                                h->invoke(true);
                            }
                        }
                    } else {
                        // 加入等待队列
                        auto wrapper = [completed, timer, handler_ptr]() mutable {
                            if (!completed->exchange(true, std::memory_order_relaxed)) {
                                timer->cancel();
                                if (*handler_ptr) {
                                    auto h = std::move(*handler_ptr);
                                    h->invoke(true);
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
     * @brief 触发事件并唤醒所有等待者
     * 
     * 注意：这是广播操作，所有等待者都会被唤醒
     * 
     * ⚠️ 重要：这是异步操作，函数会立即返回
     * 
     * 与 std::condition_variable::notify_all() 不同：
     * - 此方法是异步的（post 到 strand）
     * - 实际的通知会延迟执行
     * 
     * 如果在 notify_all() 后立即调用 reset()，由于两者都是异步的，
     * 执行顺序取决于 strand 的调度顺序。
     * 
     * 正确用法（如果需要确保顺序）：
     * @code
     * event->notify_all();
     * // 使用异步API确保顺序
     * co_await event->async_is_set(use_awaitable);  // 等待状态更新
     * event->reset();
     * @endcode
     */
    void notify_all() {
        asio::post(strand_, [this]() {
            if (is_set_) {
                return;  // 已经触发，避免重复
            }
            
            is_set_ = true;

            // 唤醒所有等待者
            while (!waiters_.empty()) {
                auto handler = std::move(waiters_.front());
                waiters_.pop_front();
                handler->invoke();
            }
        });
    }

    /**
     * @brief 重置事件状态为未触发
     * 
     * ⚠️ 重要：这是异步操作，函数会立即返回
     * 
     * 如果在 notify_all() 后立即调用 reset()，
     * 由于两者都是异步的，执行顺序不确定。
     * 
     * 如果需要确保顺序，使用异步API：
     * @code
     * event->notify_all();
     * co_await event->async_is_set(use_awaitable);
     * event->reset();
     * @endcode
     */
    void reset() {
        asio::post(strand_, [this]() {
            is_set_ = false;
        });
    }

    /**
     * @brief 检查事件是否已触发
     */
    template<typename CompletionToken = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>>
    auto async_is_set(CompletionToken&& token = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>{}) {
        return asio::async_initiate<CompletionToken, void(bool)>(
            [this](auto handler) {
                asio::post(strand_, [this, handler = std::move(handler)]() mutable {
                    std::move(handler)(is_set_);
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
            [this](auto handler) {
                asio::post(strand_, [this, handler = std::move(handler)]() mutable {
                    std::move(handler)(waiters_.size());
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
