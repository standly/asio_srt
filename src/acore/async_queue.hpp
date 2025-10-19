//
// Simplified async_queue using async_semaphore
//
#pragma once

#include "async_semaphore.hpp"
#include <asio.hpp>
#include <deque>
#include <memory>
#include <vector>
#include <chrono>
#include <cassert>

namespace acore {

/**
 * @brief 简化版异步队列 - 使用 async_semaphore 实现
 * 
 * 核心思想：
 * - semaphore 的计数 = 队列中的消息数量
 * - push() → release() 增加计数
 * - read() → acquire() 等待计数 > 0
 * 
 * 性能优化：
 * - 队列和信号量共享同一个 strand，消除跨 strand 开销
 * - 这使得 semaphore 回调可以直接访问队列，无需额外 post
 * - 相比独立 strand 设计，减少了一次异步调度延迟
 * 
 * 设计原则：
 * - 使用 strand 保证线程安全（所有共享状态都在 strand 内访问）
 * - atomic 仅用于需要立即返回的场景（如 ID 生成）
 */
template <typename T>
class async_queue : public std::enable_shared_from_this<async_queue<T>> {
public:
    // 禁止拷贝和移动（设计上通过 shared_ptr 使用）
    async_queue(const async_queue&) = delete;
    async_queue& operator=(const async_queue&) = delete;
    async_queue(async_queue&&) = delete;
    async_queue& operator=(async_queue&&) = delete;

    explicit async_queue(asio::io_context& io_context)
        : io_context_(io_context)
        , strand_(asio::make_strand(io_context.get_executor()))
        , queue_()
        , stopped_(false)
        , semaphore_(strand_, 0)  // 共享 strand，初始计数为 0（必须最后初始化）
    {}

    /**
     * @brief 推送消息到队列
     * 
     * 如果队列已停止，消息会被静默忽略
     */
    void push(T msg) {
        asio::post(strand_, [self = this->shared_from_this(), msg = std::move(msg)]() mutable {
            if (self->stopped_) return;  // 静默忽略
            
            self->queue_.push_back(std::move(msg));
            self->semaphore_.release();  // 唤醒一个等待者
        });
    }

    /**
     * @brief 批量推送
     */
    void push_batch(std::vector<T> messages) {
        if (messages.empty()) return;
        
        size_t count = messages.size();
        asio::post(strand_, [self = this->shared_from_this(), 
                             messages = std::move(messages), count]() mutable {
            if (self->stopped_) return;
            
            for (auto& msg : messages) {
                self->queue_.push_back(std::move(msg));
            }
            self->semaphore_.release(count);  // 批量唤醒
        });
    }

    /**
     * @brief 异步读取一条消息
     * 
     * 性能说明：由于 semaphore 和 queue 共享 strand，
     * acquire 完成后的回调直接在 strand 上执行，无需额外 post
     * 
     * 重要：completion handler 在内部 strand 上执行，而不是在调用者的 executor 上。
     * 如果你需要在特定的 executor（如协程的 strand）上执行，请在 handler 中
     * 使用 asio::post 切换到目标 executor。
     * 
     * 示例：
     * @code
     * queue->async_read_msg([my_strand](std::error_code ec, T msg) {
     *     asio::post(my_strand, [ec, msg = std::move(msg)]() mutable {
     *         // 现在在 my_strand 上执行
     *         process(std::move(msg));
     *     });
     * });
     * @endcode
     */
    template<typename CompletionToken = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>>
    auto async_read_msg(CompletionToken&& token = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>{}) {
        return asio::async_initiate<CompletionToken, void(std::error_code, T)>(
            [self = this->shared_from_this()](auto handler) mutable {
                // 等待 semaphore（确保有消息）
                // 回调已经在共享的 strand 上执行，可以直接访问 queue_
                self->semaphore_.acquire(
                    [self, handler = std::move(handler)](auto...) mutable {
                        // 注意：这个回调已经在 strand 上执行，可以直接访问成员变量
                        if (self->stopped_ || self->queue_.empty()) {
                            handler(std::make_error_code(std::errc::operation_canceled), T{});
                            return;
                        }
                        
                        T msg = std::move(self->queue_.front());
                        self->queue_.pop_front();
                        handler(std::error_code{}, std::move(msg));
                    }
                );
            },
            token
        );
    }

    /**
     * @brief 异步读取多条消息（高性能批量读取）
     * 
     * 等待至少一条消息，然后尽可能多地读取最多 max_count 条消息
     * 
     * 工作原理：
     * 1. 等待第一条消息（阻塞 acquire）
     * 2. 尝试批量获取更多 semaphore 计数（非阻塞）
     * 3. 批量读取队列中的消息
     * 
     * 性能特点：
     * - 只有第一条消息需要等待
     * - 后续消息批量读取，无额外等待
     * - 共享 strand：所有操作在同一个 strand 上，无跨 strand 开销
     * 
     * @param max_count 最多读取的消息数量
     * @return vector<T> 实际读取的消息（至少1条，最多max_count条）
     */
    template<typename CompletionToken = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>>
    auto async_read_msgs(size_t max_count, CompletionToken&& token = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>{}) {
        return asio::async_initiate<CompletionToken, void(std::error_code, std::vector<T>)>(
            [self = this->shared_from_this(), max_count](auto handler) mutable {
                // 先获取第一个 semaphore 计数（等待第一条消息）
                self->semaphore_.acquire(
                    [self, max_count, handler = std::move(handler)](auto...) mutable {
                        // 第一条消息已经保证存在
                        // 现在尝试批量获取更多（已经在共享的 strand 内）
                        self->semaphore_.async_try_acquire_n(
                            max_count - 1,  // 已经获取了1个，再尝试获取 max_count-1 个
                            [self, handler = std::move(handler)](size_t additional_acquired) mutable {
                                // total = 1 (第一个) + additional_acquired
                                size_t total = 1 + additional_acquired;
                                
                                // 已经在共享 strand 上，可以直接访问队列
                                if (self->stopped_) {
                                    handler(std::make_error_code(std::errc::operation_canceled), std::vector<T>{});
                                    return;
                                }
                                
                                // 批量读取消息
                                std::vector<T> messages;
                                messages.reserve(total);
                                
                                for (size_t i = 0; i < total; ++i) {
                                    // Invariant: semaphore count 应该等于 queue size
                                    if (self->queue_.empty()) {
                                        throw std::logic_error("ACORE async_queue: semaphore/queue count mismatch");
                                    }
                                    messages.push_back(std::move(self->queue_.front()));
                                    self->queue_.pop_front();
                                }
                                
                                handler(std::error_code{}, std::move(messages));
                            }
                        );
                    }
                );
            },
            token
        );
    }

    /**
     * @brief 带超时的读取（支持取消）
     * 
     * 使用 acquire_cancellable() 来支持超时时取消等待
     */
    template<typename Duration, typename CompletionToken = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>>
    auto async_read_msg_with_timeout(Duration timeout, CompletionToken&& token = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>{}) {
        return asio::async_initiate<CompletionToken, void(std::error_code, T)>(
            [self = this->shared_from_this(), timeout](auto handler) mutable {
                auto timer = std::make_shared<asio::steady_timer>(self->io_context_);
                auto completed = std::make_shared<std::atomic<bool>>(false);
                auto waiter_id = std::make_shared<uint64_t>(0);
                
                // 使用 shared_ptr 包装 handler，避免拷贝
                auto handler_ptr = std::make_shared<decltype(handler)>(std::move(handler));
                
                // 可取消的 acquire
                *waiter_id = self->semaphore_.acquire_cancellable(
                    [self, completed, timer, handler_ptr, waiter_id]() mutable {
                        if (completed->exchange(true)) {
                            return;  // 已超时
                        }
                        
                        timer->cancel();
                        
                        // 已经在共享 strand 上，可以直接访问队列
                        if (self->stopped_ || self->queue_.empty()) {
                            (*handler_ptr)(std::make_error_code(std::errc::operation_canceled), T{});
                            return;
                        }
                        
                        T msg = std::move(self->queue_.front());
                        self->queue_.pop_front();
                        (*handler_ptr)(std::error_code{}, std::move(msg));
                    }
                );
                
                // 启动超时定时器
                timer->expires_after(timeout);
                timer->async_wait([self, completed, handler_ptr, waiter_id](const std::error_code& ec) mutable {
                    if (!ec && !completed->exchange(true)) {
                        // 超时：取消 semaphore 等待
                        self->semaphore_.cancel(*waiter_id);
                        (*handler_ptr)(std::make_error_code(std::errc::timed_out), T{});
                    }
                });
            },
            token
        );
    }

    /**
     * @brief 带超时的批量读取（支持取消）
     * 
     * 等待至少一条消息（带超时），然后尽可能多地读取最多 max_count 条消息
     * 
     * 工作原理：
     * 1. 等待第一条消息（带超时，可取消）
     * 2. 如果成功获取，批量获取更多（非阻塞）
     * 3. 批量读取队列中的消息
     * 
     * @param max_count 最多读取的消息数量
     * @param timeout 超时时长
     * @return (error_code, vector<T>) - 如果超时，返回 errc::timed_out 和空 vector
     */
    template<typename Duration, typename CompletionToken = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>>
    auto async_read_msgs_with_timeout(size_t max_count, Duration timeout, CompletionToken&& token = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>{}) {
        return asio::async_initiate<CompletionToken, void(std::error_code, std::vector<T>)>(
            [self = this->shared_from_this(), max_count, timeout](auto handler) mutable {
                auto timer = std::make_shared<asio::steady_timer>(self->io_context_);
                auto completed = std::make_shared<std::atomic<bool>>(false);
                auto waiter_id = std::make_shared<uint64_t>(0);
                
                // 使用 shared_ptr 包装 handler，避免拷贝
                auto handler_ptr = std::make_shared<decltype(handler)>(std::move(handler));
                
                // 可取消的 acquire（等待第一条消息）
                *waiter_id = self->semaphore_.acquire_cancellable(
                    [self, max_count, completed, timer, handler_ptr, waiter_id]() mutable {
                        if (completed->exchange(true)) {
                            return;  // 已超时
                        }
                        
                        timer->cancel();
                        
                        // 成功获取第一条消息，尝试批量获取更多
                        self->semaphore_.async_try_acquire_n(
                            max_count - 1,  // 已经获取了1个，再尝试获取 max_count-1 个
                            [self, handler_ptr](size_t additional_acquired) mutable {
                                // total = 1 (第一个) + additional_acquired
                                size_t total = 1 + additional_acquired;
                                
                                // 已经在共享 strand 上，可以直接访问队列
                                if (self->stopped_) {
                                    (*handler_ptr)(std::make_error_code(std::errc::operation_canceled), std::vector<T>{});
                                    return;
                                }
                                
                                // 批量读取消息
                                std::vector<T> messages;
                                messages.reserve(total);
                                
                                for (size_t i = 0; i < total; ++i) {
                                    // Invariant: semaphore count 应该等于 queue size
                                    if (self->queue_.empty()) {
                                        throw std::logic_error("ACORE async_queue: semaphore/queue count mismatch");
                                    }
                                    messages.push_back(std::move(self->queue_.front()));
                                    self->queue_.pop_front();
                                }
                                
                                (*handler_ptr)(std::error_code{}, std::move(messages));
                            }
                        );
                    }
                );
                
                // 启动超时定时器
                timer->expires_after(timeout);
                timer->async_wait([self, completed, handler_ptr, waiter_id](const std::error_code& ec) mutable {
                    if (!ec && !completed->exchange(true)) {
                        // 超时：取消 semaphore 等待
                        self->semaphore_.cancel(*waiter_id);
                        (*handler_ptr)(std::make_error_code(std::errc::timed_out), std::vector<T>{});
                    }
                });
            },
            token
        );
    }

    /**
     * @brief 停止队列
     * 
     * 行为：
     * - 设置 stopped_ 标志，阻止后续的 push 和 read
     * - 取消所有等待中的 read 操作
     * - 保留队列中的残留消息（不清空）
     * 
     * 设计说明：
     * - 不清空 queue_ 是为了保持与 semaphore.count 的同步
     * - Invariant: semaphore.count + waiters.size == queue.size
     * - 如果清空 queue 但不重置 semaphore.count，会破坏这个不变量
     * - stopped_ 标志已经阻止了所有操作，残留消息无害
     * - 残留消息会在 async_queue 析构时自动清理
     */
    void stop() {
        asio::post(strand_, [self = this->shared_from_this()]() {
            self->stopped_ = true;
            // 不清空 queue_，保持与 semaphore 的 count 同步
            // 残留消息会在析构时清理
            
            // 取消所有等待的 acquire 操作
            self->semaphore_.cancel_all();
        });
    }

    /**
     * @brief 检查队列是否已停止
     */
    template<typename CompletionToken = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>>
    auto async_is_stopped(CompletionToken&& token = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>{}) {
        return asio::async_initiate<CompletionToken, void(bool)>(
            [self = this->shared_from_this()](auto handler) {
                asio::post(self->strand_, [self, handler = std::move(handler)]() mutable {
                    std::move(handler)(self->stopped_);
                });
            },
            token
        );
    }

    /**
     * @brief 获取队列大小
     */
    template<typename CompletionToken = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>>
    auto async_size(CompletionToken&& token = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>{}) {
        return asio::async_initiate<CompletionToken, void(size_t)>(
            [self = this->shared_from_this()](auto handler) {
                asio::post(self->strand_, [self, handler = std::move(handler)]() mutable {
                    std::move(handler)(self->queue_.size());
                });
            },
            token
        );
    }

private:
    asio::io_context& io_context_;
    asio::strand<asio::any_io_executor> strand_;  // 必须在 semaphore_ 之前声明（初始化顺序）
    std::deque<T> queue_;  // 仅在 strand 内访问
    bool stopped_;  // 仅在 strand 内访问，不需要 atomic
    async_semaphore semaphore_;  // ← 核心：用 semaphore 替代 pending_handler（使用共享 strand）
};

} // namespace acore


