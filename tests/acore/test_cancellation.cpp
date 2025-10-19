//
// 测试 async_semaphore 和 async_queue 的取消功能
//
#include "acore/async_queue.hpp"
#include "acore/async_semaphore.hpp"
#include <iostream>
#include <chrono>

using namespace std::chrono_literals;

// 测试 1: 超时取消
asio::awaitable<void> test_timeout_cancellation() {
    auto ex = co_await asio::this_coro::executor;
    auto& io_context = static_cast<asio::io_context&>(ex.context());
    
    auto queue = std::make_shared<acore::async_queue<std::string>>(io_context);
    
    std::cout << "测试 1: 超时取消\n";
    std::cout << "等待消息，超时 2 秒...\n";
    
    auto start = std::chrono::steady_clock::now();
    auto [ec, msg] = co_await queue->async_read_msg_with_timeout(
        2s, asio::as_tuple(asio::use_awaitable)
    );
    auto duration = std::chrono::steady_clock::now() - start;
    
    if (ec == std::errc::timed_out) {
        std::cout << "✓ 超时成功，耗时: " 
                  << std::chrono::duration_cast<std::chrono::milliseconds>(duration).count() 
                  << " ms\n";
    } else {
        std::cout << "✗ 期望超时，但得到: " << ec.message() << "\n";
    }
    
    std::cout << "\n";
}

// 测试 2: 超时后消息到达（验证不会错误消费）
asio::awaitable<void> test_timeout_then_message() {
    auto ex = co_await asio::this_coro::executor;
    auto& io_context = static_cast<asio::io_context&>(ex.context());
    
    auto queue = std::make_shared<acore::async_queue<std::string>>(io_context);
    
    std::cout << "测试 2: 超时后消息到达\n";
    
    // 启动一个定时器，3 秒后推送消息
    asio::steady_timer timer(io_context, 3s);
    timer.async_wait([queue](const std::error_code&) {
        std::cout << "  → 3 秒后推送消息\n";
        queue->push("delayed message");
    });
    
    // 1 秒超时
    std::cout << "  → 第一次读取，超时 1 秒...\n";
    auto [ec1, msg1] = co_await queue->async_read_msg_with_timeout(
        1s, asio::as_tuple(asio::use_awaitable)
    );
    
    if (ec1 == std::errc::timed_out) {
        std::cout << "  ✓ 第一次读取超时\n";
    } else {
        std::cout << "  ✗ 第一次读取应该超时\n";
    }
    
    // 再次读取（应该能读到消息）
    std::cout << "  → 第二次读取，超时 5 秒...\n";
    auto [ec2, msg2] = co_await queue->async_read_msg_with_timeout(
        5s, asio::as_tuple(asio::use_awaitable)
    );
    
    if (!ec2 && msg2 == "delayed message") {
        std::cout << "  ✓ 第二次读取成功: " << msg2 << "\n";
    } else {
        std::cout << "  ✗ 第二次读取失败: " << ec2.message() << "\n";
    }
    
    std::cout << "\n";
}

// 测试 3: 手动取消
asio::awaitable<void> test_manual_cancellation() {
    auto ex = co_await asio::this_coro::executor;
    auto& io_context = static_cast<asio::io_context&>(ex.context());
    
    auto sem = std::make_shared<acore::async_semaphore>(ex, 0);
    
    std::cout << "测试 3: 手动取消\n";
    std::cout << "  → 启动可取消的 acquire...\n";
    
    bool acquired = false;
    auto waiter_id = sem->acquire_cancellable([&acquired]() {
        acquired = true;
        std::cout << "  → acquire 完成\n";
    });
    
    std::cout << "  → waiter_id = " << waiter_id << "\n";
    
    // 等待 100ms 后取消
    asio::steady_timer timer(io_context, 100ms);
    co_await timer.async_wait(asio::use_awaitable);
    
    std::cout << "  → 取消 waiter_id = " << waiter_id << "\n";
    sem->cancel(waiter_id);
    
    // 再等待 100ms 看是否被调用
    timer.expires_after(100ms);
    co_await timer.async_wait(asio::use_awaitable);
    
    if (!acquired) {
        std::cout << "  ✓ acquire 已被取消，没有被调用\n";
    } else {
        std::cout << "  ✗ acquire 不应该被调用\n";
    }
    
    // 现在 release，看是否有效果
    std::cout << "  → 释放信号量...\n";
    sem->release();
    
    timer.expires_after(100ms);
    co_await timer.async_wait(asio::use_awaitable);
    
    if (!acquired) {
        std::cout << "  ✓ 已取消的等待者不会被唤醒\n";
    } else {
        std::cout << "  ✗ 已取消的等待者不应该被唤醒\n";
    }
    
    std::cout << "\n";
}

// 测试 4: 取消所有等待者
asio::awaitable<void> test_cancel_all() {
    auto ex = co_await asio::this_coro::executor;
    auto& io_context = static_cast<asio::io_context&>(ex.context());
    
    auto queue = std::make_shared<acore::async_queue<std::string>>(io_context);
    
    std::cout << "测试 4: 取消所有等待者（stop）\n";
    
    // 启动 3 个读取操作
    int completed_count = 0;
    
    std::cout << "  → 启动 3 个读取操作...\n";
    for (int i = 0; i < 3; ++i) {
        asio::co_spawn(ex, [queue, i, &completed_count]() -> asio::awaitable<void> {
            auto [ec, msg] = co_await queue->async_read_msg(
                asio::as_tuple(asio::use_awaitable)
            );
            if (ec) {
                std::cout << "    读取 " << i << " 被取消: " << ec.message() << "\n";
            } else {
                std::cout << "    读取 " << i << " 成功: " << msg << "\n";
            }
            ++completed_count;
        }, asio::detached);
    }
    
    // 等待一下确保它们都在等待
    asio::steady_timer timer(io_context, 100ms);
    co_await timer.async_wait(asio::use_awaitable);
    
    // 停止队列
    std::cout << "  → 调用 stop()...\n";
    queue->stop();
    
    // 等待所有读取完成
    timer.expires_after(100ms);
    co_await timer.async_wait(asio::use_awaitable);
    
    if (completed_count == 3) {
        std::cout << "  ✓ 所有读取操作都被取消了\n";
    } else {
        std::cout << "  ✗ 只有 " << completed_count << " 个读取完成\n";
    }
    
    std::cout << "\n";
}

int main() {
    try {
        asio::io_context io_context;
        
        asio::co_spawn(io_context, []() -> asio::awaitable<void> {
            co_await test_timeout_cancellation();
            co_await test_timeout_then_message();
            co_await test_manual_cancellation();
            co_await test_cancel_all();
            
            std::cout << "所有测试完成！\n";
        }, asio::detached);
        
        io_context.run();
        
    } catch (const std::exception& e) {
        std::cerr << "异常: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}

