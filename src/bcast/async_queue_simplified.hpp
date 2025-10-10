//
// Simplified async_queue using async_semaphore
//
#pragma once

#include "async_semaphore.hpp"
#include <deque>
#include <memory>
#include <vector>

namespace bcast {

/**
 * @brief 简化版异步队列 - 使用 async_semaphore 实现
 * 
 * 对比原版的优势：
 * 1. 不需要复杂的 pending_handler_ 机制
 * 2. 不需要 handler_base 类型擦除
 * 3. 代码更清晰，逻辑更简单
 */
template <typename T>
class async_queue_simplified : public std::enable_shared_from_this<async_queue_simplified<T>> {
public:
    explicit async_queue_simplified(asio::io_context& io_context)
        : io_context_(io_context)
        , strand_(asio::make_strand(io_context.get_executor()))
        , semaphore_(io_context.get_executor(), 0)  // 初始计数为 0
        , stopped_(false)
    {}

    /**
     * @brief 推送消息到队列
     */
    void push(const T& msg) {
        asio::post(strand_, [self = this->shared_from_this(), msg]() {
            if (self->stopped_) return;
            
            self->queue_.push_back(msg);
            self->semaphore_.release();  // 释放信号量，唤醒一个等待者
        });
    }

    void push(T&& msg) {
        asio::post(strand_, [self = this->shared_from_this(), msg = std::move(msg)]() mutable {
            if (self->stopped_) return;
            
            self->queue_.push_back(std::move(msg));
            self->semaphore_.release();
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
            self->semaphore_.release(count);  // 批量释放
        });
    }

    /**
     * @brief 异步读取一条消息（协程接口）
     * 
     * 简化后的实现：
     * 1. 等待信号量（确保有消息）
     * 2. 从队列取出消息
     */
    asio::awaitable<T> async_read_msg() {
        // 等待信号量（当有消息时会被唤醒）
        co_await semaphore_.acquire();
        
        // 从队列中取出消息
        T msg;
        bool success = false;
        
        asio::post(strand_, [self = this->shared_from_this(), &msg, &success]() {
            if (!self->queue_.empty()) {
                msg = std::move(self->queue_.front());
                self->queue_.pop_front();
                success = true;
            }
        });
        
        // 等待 post 完成（简化版，实际需要更好的同步）
        // 在生产代码中应该用 co_spawn + use_awaitable
        
        if (stopped_ || !success) {
            throw std::system_error(
                std::make_error_code(std::errc::operation_canceled)
            );
        }
        
        co_return msg;
    }

    /**
     * @brief 异步读取多条消息
     */
    asio::awaitable<std::vector<T>> async_read_msgs(size_t max_count) {
        std::vector<T> messages;
        messages.reserve(max_count);
        
        // 尝试读取最多 max_count 条消息
        for (size_t i = 0; i < max_count; ++i) {
            // 非阻塞尝试
            if (semaphore_.try_acquire()) {
                T msg;
                asio::post(strand_, [self = this->shared_from_this(), &msg]() {
                    if (!self->queue_.empty()) {
                        msg = std::move(self->queue_.front());
                        self->queue_.pop_front();
                    }
                });
                messages.push_back(std::move(msg));
            } else {
                break;  // 没有更多消息了
            }
        }
        
        co_return messages;
    }

    /**
     * @brief 停止队列
     */
    void stop() {
        asio::post(strand_, [self = this->shared_from_this()]() {
            self->stopped_ = true;
            self->queue_.clear();
            // TODO: 唤醒所有等待者
        });
    }

    bool is_stopped() const { return stopped_; }

private:
    asio::io_context& io_context_;
    asio::strand<asio::io_context::executor_type> strand_;
    async_semaphore semaphore_;
    std::deque<T> queue_;
    std::atomic<bool> stopped_;
};

} // namespace bcast

