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

namespace bcast {

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
class async_queue_v2 : public std::enable_shared_from_this<async_queue_v2<T>> {
public:
    explicit async_queue_v2(asio::io_context& io_context)
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
     * @brief 异步读取多条消息
     * 
     * 简化：批量 acquire
     */
    template<typename CompletionToken = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>>
    auto async_read_msgs(size_t max_count, CompletionToken&& token = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>{}) {
        return asio::async_initiate<CompletionToken, void(std::error_code, std::vector<T>)>(
            [self = this->shared_from_this(), max_count](auto handler) mutable {
                asio::post(self->strand_, [self, max_count, handler = std::move(handler)]() mutable {
                    if (self->stopped_) {
                        handler(std::make_error_code(std::errc::operation_canceled), std::vector<T>{});
                        return;
                    }
                    
                    // 批量读取（非阻塞，尽可能多）
                    std::vector<T> messages;
                    messages.reserve(std::min(max_count, self->queue_.size()));
                    
                    for (size_t i = 0; i < max_count && !self->queue_.empty(); ++i) {
                        // 尝试获取 semaphore
                        bool acquired = self->semaphore_.try_acquire(
                            [](bool success) { /* 同步版本，立即返回 */ }
                        );
                        
                        if (acquired && !self->queue_.empty()) {
                            messages.push_back(std::move(self->queue_.front()));
                            self->queue_.pop_front();
                        } else {
                            break;
                        }
                    }
                    
                    handler(std::error_code{}, std::move(messages));
                });
            },
            token
        );
    }

    /**
     * @brief 带超时的读取
     * 
     * 简化：使用 semaphore 的 try_acquire + timer
     */
    template<typename Duration, typename CompletionToken = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>>
    auto async_read_msg_with_timeout(Duration timeout, CompletionToken&& token = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>{}) {
        return asio::async_initiate<CompletionToken, void(std::error_code, T)>(
            [self = this->shared_from_this(), timeout](auto handler) mutable {
                auto timer = std::make_shared<asio::steady_timer>(self->io_context_);
                auto completed = std::make_shared<std::atomic<bool>>(false);
                
                // 启动超时定时器
                timer->expires_after(timeout);
                timer->async_wait([completed, handler](const std::error_code& ec) mutable {
                    if (!ec && !completed->exchange(true)) {
                        handler(std::make_error_code(std::errc::timed_out), T{});
                    }
                });
                
                // 尝试获取消息
                self->semaphore_.acquire(
                    [self, completed, timer, handler = std::move(handler)](auto...) mutable {
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
            },
            token
        );
    }

    /**
     * @brief 停止队列
     * 
     * 注意：需要唤醒所有等待者
     */
    void stop() {
        asio::post(strand_, [self = this->shared_from_this()]() {
            self->stopped_ = true;
            self->queue_.clear();
            
            // TODO: 需要从 semaphore 中唤醒所有等待者
            // 这是一个局限：semaphore 没有 "stop all" 机制
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
    std::deque<T> queue_;
    std::atomic<bool> stopped_;
};

} // namespace bcast

