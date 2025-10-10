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
 * 3. 线程安全且无锁
 */
class async_semaphore {
private:
    using executor_type = asio::any_io_executor;

    asio::strand<executor_type> strand_;
    std::deque<std::coroutine_handle<>> waiters_;
    size_t count_;

    struct awaiter {
        async_semaphore& sem_;

        bool await_ready() const noexcept {
            // 如果计数 > 0，可以立即继续
            return sem_.count_ > 0;
        }

        bool await_suspend(std::coroutine_handle<> h) noexcept {
            bool should_suspend = true;
            
            asio::post(sem_.strand_, [this, h, &should_suspend]() mutable {
                if (sem_.count_ > 0) {
                    // 有可用的计数，立即恢复
                    sem_.count_--;
                    asio::post(sem_.strand_.get_inner_executor(), h);
                } else {
                    // 没有计数，加入等待队列
                    sem_.waiters_.push_back(h);
                }
            });
            
            return true;  // 总是挂起，让 strand 决定何时恢复
        }

        void await_resume() const noexcept {}
    };

public:
    explicit async_semaphore(executor_type ex, size_t initial_count = 0) 
        : strand_(asio::make_strand(ex))
        , count_(initial_count) 
    {}

    /**
     * @brief 等待信号量（减少计数或等待）
     */
    [[nodiscard]] awaiter acquire() noexcept {
        return awaiter{*this};
    }

    /**
     * @brief 释放信号量（增加计数并唤醒一个等待者）
     * 
     * 关键：只唤醒一个等待者！
     */
    void release() {
        asio::post(strand_, [this]() {
            if (!waiters_.empty()) {
                // 有等待者，唤醒第一个
                auto h = waiters_.front();
                waiters_.pop_front();
                asio::post(strand_.get_inner_executor(), h);
            } else {
                // 没有等待者，增加计数
                count_++;
            }
        });
    }

    /**
     * @brief 批量释放
     */
    void release(size_t n) {
        asio::post(strand_, [this, n]() {
            for (size_t i = 0; i < n; ++i) {
                if (!waiters_.empty()) {
                    auto h = waiters_.front();
                    waiters_.pop_front();
                    asio::post(strand_.get_inner_executor(), h);
                } else {
                    count_++;
                }
            }
        });
    }

    /**
     * @brief 尝试获取（非阻塞）
     */
    bool try_acquire() {
        bool acquired = false;
        asio::post(strand_, [this, &acquired]() {
            if (count_ > 0) {
                count_--;
                acquired = true;
            }
        });
        return acquired;
    }

    executor_type get_executor() const noexcept {
        return strand_.get_inner_executor();
    }
};

} // namespace bcast

