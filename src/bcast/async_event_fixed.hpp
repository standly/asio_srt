//
// Fixed async_event implementation
//
#pragma once

#include <asio.hpp>
#include <vector>
#include <coroutine>
#include <chrono>

namespace bcast {

// ------------------------------------
// Async Manual-Reset Event (Fixed)
// ------------------------------------
class async_event {
private:
    using executor_type = asio::any_io_executor;

    asio::strand<executor_type> strand_;
    
    struct waiter_info {
        std::coroutine_handle<> handle;
    };
    std::vector<waiter_info> waiters_;
    std::atomic<bool> is_set_{false};  // 使用 atomic 保证可见性

    // 内部类：实现 co_await 逻辑
    struct awaiter {
        async_event& event_;
        bool ready_at_start_;

        awaiter(async_event& e) : event_(e), ready_at_start_(e.is_set_.load()) {}

        bool await_ready() const noexcept {
            return ready_at_start_;
        }

        bool await_suspend(std::coroutine_handle<> h) noexcept {
            // 如果初始检查时未 set，加入等待队列
            bool expected = false;
            asio::post(event_.strand_, [this, h]() mutable {
                // 在 strand 内再次检查（双重检查）
                if (event_.is_set_.load()) {
                    // 事件已触发，立即恢复
                    asio::post(event_.strand_.get_inner_executor(), h);
                } else {
                    // 加入等待队列
                    event_.waiters_.push_back({h});
                }
            });
            return true;  // 挂起协程
        }

        void await_resume() const noexcept {}
    };

public:
    explicit async_event(executor_type ex) : strand_(asio::make_strand(ex)) {}

    // 协程等待事件触发的接口
    [[nodiscard]] awaiter wait() noexcept {
        return awaiter{*this};
    }

    /**
     * @brief 触发事件并唤醒所有等待的协程。线程安全。
     */
    void notify_all() {
        asio::post(strand_, [this]() {
            if (is_set_.load()) return;
            is_set_.store(true);

            // 唤醒所有等待的协程
            auto executor = strand_.get_inner_executor();
            for (auto& w : waiters_) {
                asio::post(executor, w.handle);
            }
            waiters_.clear();
        });
    }

    /**
     * @brief 重置事件状态为未触发。线程安全。
     */
    void reset() {
        asio::post(strand_, [this]() {
            is_set_.store(false);
        });
    }

    executor_type get_executor() const noexcept {
        return strand_.get_inner_executor();
    }
};

// ------------------------------------
// 带超时的等待函数（简化版）
// ------------------------------------
template <typename Rep, typename Period>
asio::awaitable<bool> wait_for(
    async_event& event,
    std::chrono::duration<Rep, Period> duration)
{
    using namespace std::chrono;
    
    asio::steady_timer timer(co_await asio::this_coro::executor);
    timer.expires_after(duration);
    
    // 使用 std::atomic 标志来确保只有一个操作完成
    auto completed = std::make_shared<std::atomic<bool>>(false);
    auto event_triggered = std::make_shared<bool>(false);
    
    // 启动事件等待
    asio::co_spawn(
        event.get_executor(),
        [&event, completed, event_triggered]() -> asio::awaitable<void> {
            co_await event.wait();
            if (!completed->exchange(true)) {
                *event_triggered = true;
            }
        },
        asio::detached
    );
    
    // 等待超时
    try {
        co_await timer.async_wait(asio::use_awaitable);
    } catch (const std::system_error& e) {
        // Timer 被取消了
    }
    
    // 等待一小段时间确保事件等待协程有机会完成
    if (!completed->load()) {
        asio::steady_timer delay(co_await asio::this_coro::executor);
        delay.expires_after(milliseconds(1));
        co_await delay.async_wait(asio::use_awaitable);
    }
    
    co_return *event_triggered;
}

} // namespace bcast

