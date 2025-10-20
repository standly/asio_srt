//
// Async Mutex - 异步互斥锁
//
#pragma once

#include <asio.hpp>
#include <deque>
#include <memory>
#include <atomic>
#include "handler_traits.hpp"

namespace acore {

// Type alias for default completion token (简化模板参数)
namespace detail {
    using default_token_t = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>;
}

// 前向声明
class async_mutex;

/**
 * @brief RAII 风格的异步锁守卫
 * 
 * 类似 std::unique_lock，但用于异步环境
 * 析构时自动释放锁
 */
class async_lock_guard {
public:
    async_lock_guard() = default;
    
    explicit async_lock_guard(std::shared_ptr<async_mutex> mutex)
        : mutex_(std::move(mutex))
        , locked_(true)
    {}
    
    ~async_lock_guard() {
        unlock();
    }
    
    // 禁止拷贝
    async_lock_guard(const async_lock_guard&) = delete;
    async_lock_guard& operator=(const async_lock_guard&) = delete;
    
    // 允许移动
    async_lock_guard(async_lock_guard&& other) noexcept
        : mutex_(std::move(other.mutex_))
        , locked_(other.locked_)
    {
        other.locked_ = false;
    }
    
    async_lock_guard& operator=(async_lock_guard&& other) noexcept {
        if (this != &other) {
            unlock();
            mutex_ = std::move(other.mutex_);
            locked_ = other.locked_;
            other.locked_ = false;
        }
        return *this;
    }
    
    void unlock();
    
    bool owns_lock() const noexcept {
        return locked_;
    }
    
private:
    std::shared_ptr<async_mutex> mutex_;
    bool locked_ = false;
};

/**
 * @brief 异步互斥锁
 * 
 * 特性：
 * 1. 基于协程的异步锁定
 * 2. RAII 风格的锁守卫（async_lock_guard）
 * 3. 支持超时锁定
 * 4. 线程安全（使用 strand）
 * 
 * 适用场景：
 * - 保护共享资源的独占访问
 * - 配置修改的序列化
 * - 连接池管理
 * 
 * 使用示例：
 * @code
 * auto mutex = std::make_shared<async_mutex>(ex);
 * 
 * // RAII 风格（推荐）
 * auto guard = co_await mutex->async_lock(use_awaitable);
 * modify_shared_data();
 * // guard 析构时自动释放锁
 * 
 * // 或手动管理
 * co_await mutex->lock(use_awaitable);
 * modify_shared_data();
 * mutex->unlock();
 * @endcode
 */
class async_mutex : public std::enable_shared_from_this<async_mutex> {
private:
    using executor_type = asio::any_io_executor;
    
    asio::strand<executor_type> strand_;
    std::deque<std::unique_ptr<detail::void_handler_base>> waiters_;
    bool locked_{false};  // 仅在 strand 内访问

public:
    // 禁止拷贝和移动
    async_mutex(const async_mutex&) = delete;
    async_mutex& operator=(const async_mutex&) = delete;
    async_mutex(async_mutex&&) = delete;
    async_mutex& operator=(async_mutex&&) = delete;
    
    /**
     * @brief 构造函数（创建内部 strand）
     * 
     * 使用场景：当 mutex 独立使用时
     * 
     * @param ex executor（通常是 io_context.get_executor()）
     */
    explicit async_mutex(executor_type ex)
        : strand_(asio::make_strand(ex))
    {}
    
    /**
     * @brief 构造函数（使用外部 strand）
     * 
     * 使用场景：当 mutex 与其他组件共享 strand 时
     * 
     * 性能优势：
     * - 避免跨 strand 的 post 开销
     * - 多个相关组件可以在同一个 strand 上高效协作
     * 
     * 示例：
     * @code
     * auto shared_strand = asio::make_strand(io_context);
     * auto mutex = std::make_shared<async_mutex>(shared_strand);
     * auto queue = std::make_shared<async_queue<int>>(shared_strand);
     * // mutex 和 queue 共享 strand，零开销协作
     * @endcode
     * 
     * @param strand 外部提供的 strand
     */
    explicit async_mutex(asio::strand<executor_type> strand)
        : strand_(strand)
    {}
    
    /**
     * @brief 异步锁定并返回 RAII 守卫（推荐）
     * 
     * 返回 async_lock_guard，析构时自动释放锁
     * 
     * 用法：
     * @code
     * auto guard = co_await mutex->async_lock(use_awaitable);
     * // ... critical section ...
     * // guard 析构时自动 unlock
     * @endcode
     */
    template<typename CompletionToken = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>>
    auto async_lock(CompletionToken&& token = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>{}) {
        return asio::async_initiate<CompletionToken, void(async_lock_guard)>(
            [self = shared_from_this()](auto handler) {
                asio::post(self->strand_, [self, handler = std::move(handler)]() mutable {
                    if (!self->locked_) {
                        // 锁可用，立即获取
                        self->locked_ = true;
                        std::move(handler)(async_lock_guard{self});
                    } else {
                        // 锁被占用，加入等待队列
                        auto wrapper = [self, handler = std::move(handler)]() mutable {
                            std::move(handler)(async_lock_guard{self});
                        };
                        self->waiters_.push_back(
                            std::make_unique<detail::void_handler_impl<decltype(wrapper)>>(std::move(wrapper))
                        );
                    }
                });
            },
            token
        );
    }
    
    /**
     * @brief 异步锁定（无守卫版本）
     * 
     * 需要手动调用 unlock()
     * 
     * 用法：
     * @code
     * co_await mutex->lock(use_awaitable);
     * // ... critical section ...
     * mutex->unlock();  // 必须手动释放
     * @endcode
     */
    template<typename CompletionToken = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>>
    auto lock(CompletionToken&& token = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>{}) {
        return asio::async_initiate<CompletionToken, void()>(
            [self = shared_from_this()](auto handler) {
                asio::post(self->strand_, [self, handler = std::move(handler)]() mutable {
                    if (!self->locked_) {
                        // 锁可用，立即获取
                        self->locked_ = true;
                        std::move(handler)();
                    } else {
                        // 锁被占用，加入等待队列
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
     * @brief 带超时的锁定
     * 
     * @return true - 成功获取锁
     *         false - 超时
     * 
     * 用法：
     * @code
     * if (co_await mutex->try_lock_for(5s, use_awaitable)) {
     *     // 成功获取锁
     *     modify_data();
     *     mutex->unlock();
     * } else {
     *     // 超时
     * }
     * @endcode
     */
    template<typename Rep, typename Period, typename CompletionToken = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>>
    auto try_lock_for(
        std::chrono::duration<Rep, Period> timeout,
        CompletionToken&& token = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>{})
    {
        return asio::async_initiate<CompletionToken, void(bool)>(
            [self = shared_from_this(), timeout](auto handler) {
                auto completed = std::make_shared<std::atomic<bool>>(false);
                auto timer = std::make_shared<asio::steady_timer>(self->strand_.get_inner_executor());
                
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
                
                // 尝试锁定
                asio::post(self->strand_, [self, completed, timer, handler_ptr]() mutable {
                    if (!self->locked_) {
                        // 锁可用，立即获取
                        self->locked_ = true;
                        if (!completed->exchange(true, std::memory_order_relaxed)) {
                            timer->cancel();
                            if (*handler_ptr) {
                                auto h = std::move(*handler_ptr);
                                h->invoke(true);  // true = 成功
                            }
                        }
                    } else {
                        // 加入等待队列（带超时取消）
                        auto wrapper = [completed, timer, handler_ptr]() mutable {
                            if (!completed->exchange(true, std::memory_order_relaxed)) {
                                timer->cancel();
                                if (*handler_ptr) {
                                    auto h = std::move(*handler_ptr);
                                    h->invoke(true);  // true = 成功
                                }
                            }
                        };
                        self->waiters_.push_back(
                            std::make_unique<detail::void_handler_impl<decltype(wrapper)>>(std::move(wrapper))
                        );
                    }
                });
            },
            token
        );
    }
    
    /**
     * @brief 释放锁
     * 
     * 如果有等待者，唤醒队首的等待者
     * 否则，标记锁为未锁定状态
     */
    void unlock() {
        asio::post(strand_, [self = shared_from_this()]() {
            if (!self->locked_) {
                // 已经解锁，忽略（防止重复 unlock）
                return;
            }
            
            if (!self->waiters_.empty()) {
                // 有等待者，唤醒第一个
                auto handler = std::move(self->waiters_.front());
                self->waiters_.pop_front();
                // locked_ 保持为 true，因为锁传给了下一个等待者
                handler->invoke();
            } else {
                // 没有等待者，释放锁
                self->locked_ = false;
            }
        });
    }
    
    /**
     * @brief 检查锁是否被持有
     */
    template<typename CompletionToken = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>>
    auto async_is_locked(CompletionToken&& token = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>{}) {
        return asio::async_initiate<CompletionToken, void(bool)>(
            [self = shared_from_this()](auto handler) {
                asio::post(self->strand_, [self, handler = std::move(handler)]() mutable {
                    std::move(handler)(self->locked_);
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

// async_lock_guard 实现
inline void async_lock_guard::unlock() {
    if (locked_ && mutex_) {
        mutex_->unlock();
        locked_ = false;
    }
}

} // namespace acore

