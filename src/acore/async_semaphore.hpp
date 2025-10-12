//
// Async Semaphore - 适合用于 async_queue
//
#pragma once

#include <asio.hpp>
#include <deque>
#include <coroutine>
#include <atomic>
#include <memory>

namespace bcast {

/**
 * @brief 异步信号量 - 每次 release 只唤醒一个等待者
 * 
 * 这个类适合用于实现 async_queue，因为：
 * 1. release() 只唤醒一个等待者（而不是全部）
 * 2. 支持计数（可以表示队列中的消息数量）
 * 3. 线程安全且无锁（使用 strand 保护）
 * 
 * 注意：所有操作都是异步的，立即返回
 */
class async_semaphore {
private:
    using executor_type = asio::any_io_executor;

    asio::strand<executor_type> strand_;
    
    // 类型擦除的 handler 接口（支持取消）
    struct handler_base {
        uint64_t id_;                // 唯一 ID
        bool cancelled_ = false;     // 标记是否已取消
        
        explicit handler_base(uint64_t id) : id_(id) {}
        virtual ~handler_base() = default;
        virtual void invoke() = 0;
        virtual void cancel() = 0;  // 添加取消接口
    };
    
    template<typename Handler>
    struct handler_impl : handler_base {
        Handler handler_;
        
        handler_impl(uint64_t id, Handler&& h) 
            : handler_base(id), handler_(std::move(h)) {}
        
        void invoke() override {
            if (!cancelled_) {
                std::move(handler_)();
            }
        }
        
        void cancel() override {
            if (!cancelled_) {
                cancelled_ = true;
                // 不调用 handler，只是标记为取消
                // handler 会被析构
            }
        }
    };
    
    std::deque<std::unique_ptr<handler_base>> waiters_;
    std::atomic<size_t> count_;  // 使用原子变量，允许无锁读取
    std::atomic<uint64_t> next_id_{1};  // 从 1 开始，0 表示无效/立即完成

public:
    explicit async_semaphore(executor_type ex, size_t initial_count = 0) 
        : strand_(asio::make_strand(ex))
        , count_(initial_count) 
    {}

    /**
     * @brief 等待信号量（减少计数或等待）
     * 
     * 用法：co_await sem.acquire();
     */
    template<typename CompletionToken = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>>
    auto acquire(CompletionToken&& token = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>{}) {
        return asio::async_initiate<CompletionToken, void()>(
            [this](auto handler) {
                asio::post(strand_, [this, handler = std::move(handler)]() mutable {
                    if (count_.load(std::memory_order_acquire) > 0) {
                        // 有可用的计数，立即完成
                        count_.fetch_sub(1, std::memory_order_release);
                        std::move(handler)();
                    } else {
                        // 没有计数，加入等待队列
                        uint64_t waiter_id = next_id_.fetch_add(1, std::memory_order_relaxed);
                        waiters_.push_back(
                            std::make_unique<handler_impl<decltype(handler)>>(waiter_id, std::move(handler))
                        );
                    }
                });
            },
            token
        );
    }
    
    /**
     * @brief 可取消的等待信号量
     * 
     * 返回一个 waiter_id，可用于取消操作
     * 如果立即获取成功，handler 会被调用，waiter_id = 0
     */
    template<typename Handler>
    uint64_t acquire_cancellable(Handler&& handler) {
        uint64_t waiter_id = next_id_.fetch_add(1, std::memory_order_relaxed);
        
        asio::post(strand_, [this, waiter_id, handler = std::forward<Handler>(handler)]() mutable {
            if (count_.load(std::memory_order_acquire) > 0) {
                // 有可用的计数，立即完成
                count_.fetch_sub(1, std::memory_order_release);
                std::move(handler)();
                // 需要标记这个 waiter_id 已完成（从外部看就像取消了）
            } else {
                // 没有计数，加入等待队列
                waiters_.push_back(
                    std::make_unique<handler_impl<decltype(handler)>>(waiter_id, std::move(handler))
                );
            }
        });
        
        return waiter_id;
    }

    /**
     * @brief 释放信号量（增加计数并唤醒一个等待者）
     * 
     * 关键：只唤醒一个等待者！
     * 
     * 注意：这是异步操作，函数会立即返回
     * 实际的释放操作在 strand 上串行执行
     */
    void release() {
        asio::post(strand_, [this]() {
            if (!waiters_.empty()) {
                // 有等待者，直接唤醒第一个（不增加计数）
                auto handler = std::move(waiters_.front());
                waiters_.pop_front();
                // 调用 handler（如果已取消，invoke() 不会做任何事）
                handler->invoke();
            } else {
                // 没有等待者，增加计数
                count_.fetch_add(1, std::memory_order_release);
            }
        });
    }

    /**
     * @brief 批量释放
     * 
     * 注意：这是异步操作，函数会立即返回
     */
    void release(size_t n) {
        if (n == 0) return;
        
        asio::post(strand_, [this, n]() {
            for (size_t i = 0; i < n; ++i) {
                if (!waiters_.empty()) {
                    auto handler = std::move(waiters_.front());
                    waiters_.pop_front();
                    handler->invoke();
                } else {
                    count_.fetch_add(1, std::memory_order_release);
                }
            }
        });
    }
    
    /**
     * @brief 取消指定的等待操作
     * 
     * @param waiter_id 由 acquire_cancellable() 返回的 ID
     * 
     * 注意：这是异步操作，函数会立即返回
     * 直接从队列中移除等待者，立即释放资源
     */
    void cancel(uint64_t waiter_id) {
        if (waiter_id == 0) return;  // 0 表示无效 ID
        
        asio::post(strand_, [this, waiter_id]() {
            // 直接移除，不只是标记
            for (auto it = waiters_.begin(); it != waiters_.end(); ++it) {
                if ((*it)->id_ == waiter_id) {
                    waiters_.erase(it);  // 立即移除，释放 handler
                    return;
                }
            }
        });
    }
    
    /**
     * @brief 取消所有等待操作
     * 
     * 用于 stop() 场景：清空所有等待者，不调用它们的 handler
     * 
     * 注意：这是异步操作，函数会立即返回
     */
    void cancel_all() {
        asio::post(strand_, [this]() {
            waiters_.clear();  // 直接清空，所有 handler 析构
        });
    }

    /**
     * @brief 尝试获取（非阻塞）
     * 
     * 返回一个 awaitable，结果为 bool：
     * - true: 成功获取
     * - false: 当前没有可用的计数
     * 
     * 用法：bool success = co_await sem.try_acquire();
     */
    template<typename CompletionToken = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>>
    auto try_acquire(CompletionToken&& token = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>{}) {
        return asio::async_initiate<CompletionToken, void(bool)>(
            [this](auto handler) {
                asio::post(strand_, [this, handler = std::move(handler)]() mutable {
                    bool success = false;
                    if (count_.load(std::memory_order_acquire) > 0) {
                        count_.fetch_sub(1, std::memory_order_release);
                        success = true;
                    }
                    // 调用 completion handler
                    std::move(handler)(success);
                });
            },
            token
        );
    }

    /**
     * @brief 获取当前计数（仅用于调试/监控）
     * 
     * 注意：由于并发性，返回的值可能立即过时
     */
    size_t count() const noexcept {
        return count_.load(std::memory_order_acquire);
    }

    /**
     * @brief 获取等待者数量（仅用于调试/监控）
     * 
     * 注意：这需要在 strand 上执行，因此是异步的
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
