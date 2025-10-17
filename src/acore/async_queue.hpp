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

namespace acore {

/**
 * @brief 简化版异步队列 - 使用 async_semaphore 实现
 * 
 * 相比原版 async_queue 的优势：
 * 1. 不需要 pending_handler_ 机制
 * 2. 不需要 handler_base 类型擦除（semaphore 内部已处理）
 * 3. 代码更简洁，逻辑更清晰
 * 4. 自动支持多个等待者（semaphore 内部管理）
 * 
 * 核心思想：
 * - semaphore 的计数 = 队列中的消息数量
 * - push() → release() 增加计数
 * - read() → acquire() 等待计数 > 0
 */
template <typename T>
class async_queue : public std::enable_shared_from_this<async_queue<T>> {
public:
    explicit async_queue(asio::io_context& io_context)
        : io_context_(io_context)
        , strand_(asio::make_strand(io_context.get_executor()))
        , semaphore_(io_context.get_executor(), 0)  // 初始计数为 0
        , stopped_(false)
    {}

    /**
     * @brief 推送消息到队列
     * 
     * 简化：不需要检查 pending_handler，semaphore 会自动唤醒等待者
     */
    void push(T msg) {
        asio::post(strand_, [self = this->shared_from_this(), msg = std::move(msg)]() mutable {
            if (self->stopped_) return;
            
            self->queue_.push_back(std::move(msg));
            self->semaphore_.release();  // 唤醒一个等待者
        });
    }

    /**
     * @brief 批量推送
     * 
     * 简化：直接批量 release
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
     * 简化：先 acquire semaphore，再从队列取消息
     */
    template<typename CompletionToken = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>>
    auto async_read_msg(CompletionToken&& token = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>{}) {
        return asio::async_initiate<CompletionToken, void(std::error_code, T)>(
            [self = this->shared_from_this()](auto handler) mutable {
                // 先等待 semaphore（确保有消息）
                self->semaphore_.acquire(
                    [self, handler = std::move(handler)](auto...) mutable {
                        // semaphore 已获取，从队列取消息
                        asio::post(self->strand_, [self, handler = std::move(handler)]() mutable {
                            if (self->stopped_ || self->queue_.empty()) {
                                handler(std::make_error_code(std::errc::operation_canceled), T{});
                                return;
                            }
                            
                            T msg = std::move(self->queue_.front());
                            self->queue_.pop_front();
                            handler(std::error_code{}, std::move(msg));
                        });
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
     * - 线程安全：所有队列操作在 queue 的 strand 内完成
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
                        // 现在尝试批量获取更多（在 semaphore 的 strand 内）
                        self->semaphore_.async_try_acquire_n(
                            max_count - 1,  // 已经获取了1个，再尝试获取 max_count-1 个
                            [self, handler = std::move(handler)](size_t additional_acquired) mutable {
                                // total = 1 (第一个) + additional_acquired
                                size_t total = 1 + additional_acquired;
                                
                                // 切换到 queue 的 strand 来操作队列
                                asio::post(self->strand_, [self, total, handler = std::move(handler)]() mutable {
                                    if (self->stopped_) {
                                        handler(std::make_error_code(std::errc::operation_canceled), std::vector<T>{});
                                        return;
                                    }
                                    
                                    // 批量读取消息
                                    std::vector<T> messages;
                                    messages.reserve(total);
                                    
                                    for (size_t i = 0; i < total && !self->queue_.empty(); ++i) {
                                        messages.push_back(std::move(self->queue_.front()));
                                        self->queue_.pop_front();
                                    }
                                    
                                    handler(std::error_code{}, std::move(messages));
                                });
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
                
                // 可取消的 acquire
                *waiter_id = self->semaphore_.acquire_cancellable(
                    [self, completed, timer, handler, waiter_id]() mutable {
                        if (completed->exchange(true)) {
                            return;  // 已超时
                        }
                        
                        timer->cancel();
                        
                        asio::post(self->strand_, [self, handler = std::move(handler)]() mutable {
                            if (self->stopped_ || self->queue_.empty()) {
                                handler(std::make_error_code(std::errc::operation_canceled), T{});
                                return;
                            }
                            
                            T msg = std::move(self->queue_.front());
                            self->queue_.pop_front();
                            handler(std::error_code{}, std::move(msg));
                        });
                    }
                );
                
                // 启动超时定时器
                timer->expires_after(timeout);
                timer->async_wait([self, completed, handler, waiter_id](const std::error_code& ec) mutable {
                    if (!ec && !completed->exchange(true)) {
                        // 超时：取消 semaphore 等待
                        self->semaphore_.cancel(*waiter_id);
                        handler(std::make_error_code(std::errc::timed_out), T{});
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
                
                // 可取消的 acquire（等待第一条消息）
                *waiter_id = self->semaphore_.acquire_cancellable(
                    [self, max_count, completed, timer, handler, waiter_id]() mutable {
                        if (completed->exchange(true)) {
                            return;  // 已超时
                        }
                        
                        timer->cancel();
                        
                        // 成功获取第一条消息，尝试批量获取更多
                        self->semaphore_.async_try_acquire_n(
                            max_count - 1,  // 已经获取了1个，再尝试获取 max_count-1 个
                            [self, handler = std::move(handler)](size_t additional_acquired) mutable {
                                // total = 1 (第一个) + additional_acquired
                                size_t total = 1 + additional_acquired;
                                
                                // 切换到 queue 的 strand 来操作队列
                                asio::post(self->strand_, [self, total, handler = std::move(handler)]() mutable {
                                    if (self->stopped_) {
                                        handler(std::make_error_code(std::errc::operation_canceled), std::vector<T>{});
                                        return;
                                    }
                                    
                                    // 批量读取消息
                                    std::vector<T> messages;
                                    messages.reserve(total);
                                    
                                    for (size_t i = 0; i < total && !self->queue_.empty(); ++i) {
                                        messages.push_back(std::move(self->queue_.front()));
                                        self->queue_.pop_front();
                                    }
                                    
                                    handler(std::error_code{}, std::move(messages));
                                });
                            }
                        );
                    }
                );
                
                // 启动超时定时器
                timer->expires_after(timeout);
                timer->async_wait([self, completed, handler, waiter_id](const std::error_code& ec) mutable {
                    if (!ec && !completed->exchange(true)) {
                        // 超时：取消 semaphore 等待
                        self->semaphore_.cancel(*waiter_id);
                        handler(std::make_error_code(std::errc::timed_out), std::vector<T>{});
                    }
                });
            },
            token
        );
    }

    /**
     * @brief 停止队列
     * 
     * 取消所有等待的读操作
     */
    void stop() {
        asio::post(strand_, [self = this->shared_from_this()]() {
            self->stopped_ = true;
            self->queue_.clear();
            
            // 取消所有等待者
            self->semaphore_.cancel_all();
        });
    }

    bool is_stopped() const { return stopped_; }

    size_t size() const {
        // 注意：这是近似值，因为是异步的
        return queue_.size();
    }

private:
    asio::io_context& io_context_;
    asio::strand<asio::any_io_executor> strand_;
    async_semaphore semaphore_;  // ← 核心：用 semaphore 替代 pending_handler
    std::deque<T> queue_;  // 仅在 strand 内访问
    // stopped_ 使用 atomic：允许 is_stopped() 方法无锁读取
    std::atomic<bool> stopped_;
};

} // namespace acore


