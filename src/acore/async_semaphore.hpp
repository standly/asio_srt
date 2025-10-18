//
// Async Semaphore - 适合用于 async_queue
//
#pragma once

#include <asio.hpp>
#include <list>
#include <coroutine>
#include <atomic>
#include <memory>
#include <unordered_map>
#include "handler_traits.hpp"

namespace acore {

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
    
    using waiter_list = std::list<std::unique_ptr<detail::cancellable_void_handler_base>>;
    waiter_list waiters_;  // 仅在 strand 内访问
    // O(1) cancel: map from waiter_id to iterator (list iterators are stable)
    std::unordered_map<uint64_t, waiter_list::iterator> waiter_map_;  // 仅在 strand 内访问
    size_t count_;  // 仅在 strand 内访问，不需要 atomic
    // next_id_ 使用 atomic：在 strand 外生成 ID，需要线程安全
    std::atomic<uint64_t> next_id_{1};

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
                    if (count_ > 0) {
                        // 有可用的计数，立即完成
                        --count_;
                        std::move(handler)();
                    } else {
                        // 没有计数，加入等待队列 (非可取消版本，id=0)
                        waiters_.push_back(
                            std::make_unique<detail::cancellable_void_handler_base>(0, std::move(handler))
                        );
                        // 非可取消版本不加入map
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
            if (count_ > 0) {
                // 有可用的计数，立即完成
                --count_;
                std::move(handler)();
            } else {
                // 没有计数，加入等待队列
                waiters_.push_back(
                    std::make_unique<detail::cancellable_void_handler_base>(waiter_id, std::move(handler))
                );
                // O(1) cancel: 记录iterator到map (list iterator stays valid)
                waiter_map_[waiter_id] = std::prev(waiters_.end());
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
                // 从map中移除（如果是可取消的）
                if (handler->id_ != 0) {
                    waiter_map_.erase(handler->id_);
                }
                // 调用 handler（如果已取消，invoke() 不会做任何事）
                handler->invoke();
            } else {
                // 没有等待者，增加计数
                ++count_;
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
                    // 从map中移除（如果是可取消的）
                    if (handler->id_ != 0) {
                        waiter_map_.erase(handler->id_);
                    }
                    handler->invoke();
                } else {
                    ++count_;
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
     * 时间复杂度: O(1) - 使用map直接查找
     */
    void cancel(uint64_t waiter_id) {
        if (waiter_id == 0) return;  // 0 表示无效 ID
        
        asio::post(strand_, [this, waiter_id]() {
            // O(1) 查找并移除
            auto map_it = waiter_map_.find(waiter_id);
            if (map_it != waiter_map_.end()) {
                auto list_it = map_it->second;
                // 先从map移除
                waiter_map_.erase(map_it);
                // 再从list移除（会自动销毁handler，无需显式cancel）
                waiters_.erase(list_it);
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
            waiter_map_.clear();  // 清空map
        });
    }
    
    /**
     * @brief 异步尝试获取（非阻塞）
     * 
     * 返回一个 awaitable，结果为 bool：
     * - true: 成功获取
     * - false: 当前没有可用的计数
     * 
     * 注意：这个方法是异步的，虽然不会等待 semaphore，
     * 但仍需要 post 到 strand。命名为 async_try_acquire 以明确这一点。
     * 
     * 用法：bool success = co_await sem.async_try_acquire(use_awaitable);
     */
    template<typename CompletionToken = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>>
    auto async_try_acquire(CompletionToken&& token = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>{}) {
        return asio::async_initiate<CompletionToken, void(bool)>(
            [this](auto handler) {
                asio::post(strand_, [this, handler = std::move(handler)]() mutable {
                    bool success = false;
                    if (count_ > 0) {
                        --count_;
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
     * @brief 批量尝试获取（非阻塞，在 strand 回调中执行）
     * 
     * 这是一个特殊的 API，用于高性能批量操作场景（如 async_queue 的批量读取）
     * 
     * @param max_count 最多尝试获取的数量
     * @param callback 回调函数，在 strand 上下文中执行，接收实际获取的数量
     * 
     * 工作原理：
     * 1. 在 strand 内执行
     * 2. 尝试获取最多 max_count 个计数
     * 3. 返回实际获取的数量
     * 4. 调用者可以在同一个 strand 上下文中继续操作
     * 
     * 这个方法保证线程安全，因为所有操作都在 strand 内完成
     */
    template<typename Callback>
    void async_try_acquire_n(size_t max_count, Callback&& callback) {
        asio::post(strand_, [this, max_count, callback = std::forward<Callback>(callback)]() mutable {
            size_t acquired = 0;
            
            if (count_ > 0) {
                acquired = std::min(max_count, count_);
                count_ -= acquired;
            }
            
            // 在 strand 上下文中调用回调，传递实际获取的数量
            callback(acquired);
        });
    }

    /**
     * @brief 获取当前计数
     */
    template<typename CompletionToken = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>>
    auto async_count(CompletionToken&& token = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>{}) {
        return asio::async_initiate<CompletionToken, void(size_t)>(
            [this](auto handler) {
                asio::post(strand_, [this, handler = std::move(handler)]() mutable {
                    std::move(handler)(count_);
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
