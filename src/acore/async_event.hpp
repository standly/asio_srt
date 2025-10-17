//
// Async Event (Manual-Reset Event) - 参考 async_semaphore 重写
//
#pragma once

#include <asio.hpp>
#include <deque>
#include <memory>
#include <atomic>
#include <chrono>

namespace bcast {

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
    
    // 类型擦除的 handler 接口（参考 async_semaphore）
    struct handler_base {
        virtual ~handler_base() = default;
        virtual void invoke() = 0;
    };
    
    template<typename Handler>
    struct handler_impl : handler_base {
        Handler handler_;
        explicit handler_impl(Handler&& h) : handler_(std::move(h)) {}
        void invoke() override {
            std::move(handler_)();
        }
    };
    
    // 超时 handler 接口（带bool参数）
    struct timeout_handler_base {
        virtual ~timeout_handler_base() = default;
        virtual void invoke(bool result) = 0;
    };
    
    template<typename Handler>
    struct timeout_handler_impl : timeout_handler_base {
        Handler handler_;
        explicit timeout_handler_impl(Handler&& h) : handler_(std::move(h)) {}
        void invoke(bool result) override {
            std::move(handler_)(result);
        }
    };
    
    std::deque<std::unique_ptr<handler_base>> waiters_;  // 仅在 strand 内访问
    // is_set_ 使用 atomic：允许 is_set() 方法无锁读取（虽然值可能过时）
    std::atomic<bool> is_set_{false};

public:
    explicit async_event(executor_type ex) 
        : strand_(asio::make_strand(ex)) 
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
                    if (is_set_.load(std::memory_order_acquire)) {
                        // 事件已触发，立即完成
                        std::move(handler)();
                    } else {
                        // 事件未触发，加入等待队列
                        waiters_.push_back(
                            std::make_unique<handler_impl<decltype(handler)>>(std::move(handler))
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
                auto handler_ptr = std::make_shared<std::unique_ptr<timeout_handler_base>>(
                    std::make_unique<timeout_handler_impl<decltype(handler)>>(std::move(handler))
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
                    if (is_set_.load(std::memory_order_acquire)) {
                        // 事件已触发
                        if (!completed->exchange(true, std::memory_order_acq_rel)) {
                            timer->cancel();
                            if (*handler_ptr) {
                                auto h = std::move(*handler_ptr);
                                h->invoke(true);
                            }
                        }
                    } else {
                        // 加入等待队列
                        auto wrapper = [completed, timer, handler_ptr]() mutable {
                            if (!completed->exchange(true, std::memory_order_acq_rel)) {
                                timer->cancel();
                                if (*handler_ptr) {
                                    auto h = std::move(*handler_ptr);
                                    h->invoke(true);
                                }
                            }
                        };
                        waiters_.push_back(
                            std::make_unique<handler_impl<decltype(wrapper)>>(std::move(wrapper))
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
     * 这是异步操作，函数会立即返回
     */
    void notify_all() {
        asio::post(strand_, [this]() {
            if (is_set_.load(std::memory_order_acquire)) {
                return;  // 已经触发，避免重复
            }
            
            is_set_.store(true, std::memory_order_release);

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
     * 注意：这是异步操作，函数会立即返回
     */
    void reset() {
        asio::post(strand_, [this]() {
            is_set_.store(false, std::memory_order_release);
        });
    }

    /**
     * @brief 检查事件是否已触发
     * 
     * 注意：由于并发性，返回的值可能立即过时
     */
    bool is_set() const noexcept {
        return is_set_.load(std::memory_order_acquire);
    }

    /**
     * @brief 获取等待者数量
     */
    template<typename CompletionToken = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>>
    auto waiting_count(CompletionToken&& token = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>{}) {
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

} // namespace bcast
