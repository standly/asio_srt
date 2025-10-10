//
// Created by ubuntu on 10/10/25.
//
#include <asio.hpp>
#include <iostream>
#include <vector>
#include <coroutine>
#include <chrono>
#include <future> // 用于 asio::use_future

using timer_type = asio::steady_timer;

// ------------------------------------
// 1. 异步事件 (Async Manual-Reset Event)
// ------------------------------------
class async_event {
private:
    using executor_type = asio::any_io_executor;

    // 使用 strand 保证对共享状态 (waiters_, is_set_) 的串行访问
    asio::strand<executor_type> strand_;

    // 等待队列，存储协程句柄和对应的取消信号槽 (用于超时取消)
    struct waiter_info {
        std::coroutine_handle<> handle;
        // 存储取消槽，用于在事件触发时断开连接，避免已恢复协程的slot被访问
        asio::cancellation_slot slot;
    };
    std::vector<waiter_info> waiters_;
    bool is_set_ = false;

    // 内部类：实现 co_await 逻辑
    struct awaiter {
        async_event& event_;

        bool await_ready() const noexcept {
            return event_.is_set_;
        }

        void await_suspend(std::coroutine_handle<> h) noexcept {
            // 获取当前协程的取消槽
            asio::cancellation_slot slot = asio::get_associated_cancellation_slot(h);

            // 在 strand 上安全地将协程加入等待队列
            asio::post(event_.strand_, [this, h, slot]() mutable {
                // 在协程等待的过程中，如果事件已经触发，则立即恢复协程
                if (event_.is_set_) {
                    asio::post(h); // 异步恢复协程
                } else {
                    // 否则，将协程句柄和取消槽加入队列
                    event_.waiters_.push_back({h, slot});
                }
            });
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
            if (is_set_) return;
            is_set_ = true;

            // 唤醒所有等待的协程
            for (auto& w : waiters_) {
                // post(h) 异步恢复协程
                asio::post(w.handle);
            }
            waiters_.clear();
        });
    }

    /**
     * @brief 重置事件状态为未触发。线程安全。
     */
    void reset() {
        asio::post(strand_, [this]() {
            is_set_ = false;
        });
    }

    // 暴露执行器，供外部函数（如定时器）使用
    executor_type get_executor() const noexcept {
        return strand_.get_inner_executor();
    }
};

// ------------------------------------
// 2. 带超时的等待函数
// ------------------------------------

/**
 * @brief 异步等待事件，并支持超时。
 * @return asio::awaitable<bool> 返回一个 awaitable，其结果为 bool:
 * true  - 事件在超时前触发。
 * false - 等待超时。
 */
template <typename Rep, typename Period>
asio::awaitable<bool> wait_for(
    async_event& event,
    std::chrono::duration<Rep, Period> duration)
{
    // 1. 创建用于取消事件等待的信号源
    asio::cancellation_signal cancel_signal;

    // 2. 启动事件的异步等待操作，并获取 std::future
    // 我们将 cancel_signal 的 slot 绑定到此协程的执行上下文中
    auto event_wait_future = asio::co_spawn(
        event.get_executor(),
        [&event]() -> asio::awaitable<void> {
            // 协程在此处等待事件触发
            co_await event.wait();
        },
        // 传递一个 completion token，将 cancellation_slot 绑定到 future 的执行上下文
        asio::bind_cancellation_slot(cancel_signal.slot(), asio::use_future)
    );

    // 3. 启动定时器
    timer_type timer(event.get_executor());
    timer.expires_after(duration);

    // 异步等待定时器
    timer.async_wait(
        // 定时器到期回调
        [&cancel_signal](const boost::system::error_code& ec) {
            // 只有在没有被取消的情况下（即定时器真的到期了）才发送取消信号
            if (ec != asio::error::operation_aborted) {
                // 发送取消信号，这将中断 event_wait_future 对应的协程
                cancel_signal.emit(asio::cancellation_type::terminal);
            }
        }
    );

    // 4. 等待 future 完成（事件触发或被取消）
    try {
        co_await asio::async_wait(
            std::move(event_wait_future),
            asio::use_awaitable
        );

        // 如果到达这里，说明 event_wait_future 正常完成 (事件已触发)
        // 此时，我们需要取消定时器，避免它在未来触发
        timer.cancel();
        co_return true;

    } catch (const boost::system::system_error& e) {
        // 如果捕获到 operation_aborted，说明是被定时器取消了 (超时)
        if (e.code() == asio::error::operation_aborted) {
            co_return false; // 超时
        }
        throw; // 抛出其他非预期的错误
    }
}
//
// // ------------------------------------
// // 3. 示例用法 (演示超时情况)
// // ------------------------------------
// using namespace std::literals::chrono_literals;
//
// asio::awaitable<void> consumer_with_timeout(int id, async_event& event) {
//     std::cout << "[Consumer " << id << "] 启动，等待事件 (超时 1.5 秒)...\n";
//
//     // 调用带超时的等待函数
//     bool triggered = co_await wait_for(event, 1500ms);
//
//     if (triggered) {
//         std::cout << "[Consumer " << id << "] **事件已接收！** 恢复执行。\n";
//     } else {
//         std::cout << "[Consumer " << id << "] **超时了！** 事件未在 1.5 秒内触发。\n";
//     }
// }
//
// asio::awaitable<void> producer_delayed(async_event& event, asio::steady_timer& timer) {
//     // 延迟 3 秒，确保消费者超时
//     std::cout << "[Producer] 启动，等待 3 秒后触发事件...\n";
//     timer.expires_after(3s);
//     co_await timer.async_wait(asio::use_awaitable);
//
//     std::cout << "[Producer] **时间到，触发事件 (notify_all) !**\n";
//     event.notify_all();
// }
//
// int main() {
//     try {
//         asio::io_context ioc;
//
//         async_event event(ioc.get_executor());
//         asio::steady_timer timer(ioc);
//
//         asio::co_spawn(ioc, consumer_with_timeout(1, event), asio::detached);
//         asio::co_spawn(ioc, producer_delayed(event, timer), asio::detached);
//
//         std::cout << "--- 启动 Asio 事件循环 ---\n";
//         ioc.run();
//         std::cout << "--- Asio 事件循环结束 ---\n";
//
//     } catch (const std::exception& e) {
//         std::cerr << "Exception: " << e.what() << "\n";
//     }
//     return 0;
// }