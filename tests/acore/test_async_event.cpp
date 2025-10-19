//
// async_event 全面测试 - 包括竞态条件测试
//
#include "acore/async_event.hpp"
#include <iostream>
#include <chrono>
#include <atomic>

using namespace std::chrono_literals;

// 测试 1: 基本 wait/notify
asio::awaitable<void> test_basic_event() {
    auto ex = co_await asio::this_coro::executor;
    
    auto event = std::make_shared<acore::async_event>(ex);
    
    std::cout << "测试 1: 基本 wait/notify\n";
    
    bool triggered = false;
    
    // 启动等待
    asio::co_spawn(ex, [event, &triggered]() -> asio::awaitable<void> {
        std::cout << "  → 等待事件...\n";
        co_await event->wait(asio::use_awaitable);
        triggered = true;
        std::cout << "  ✓ 事件被触发\n";
    }, asio::detached);
    
    // 等待一下
    asio::steady_timer timer(ex, 100ms);
    co_await timer.async_wait(asio::use_awaitable);
    
    // Notify
    std::cout << "  → 调用 notify_all()\n";
    event->notify_all();
    
    // 等待完成
    timer.expires_after(100ms);
    co_await timer.async_wait(asio::use_awaitable);
    
    if (triggered) {
        std::cout << "  ✓ 等待者被正确唤醒\n";
    }
    
    std::cout << "\n";
}

// 测试 2: 多个等待者（广播）
asio::awaitable<void> test_broadcast() {
    auto ex = co_await asio::this_coro::executor;
    
    auto event = std::make_shared<acore::async_event>(ex);
    
    std::cout << "测试 2: 广播给多个等待者\n";
    
    std::atomic<int> triggered_count{0};
    const int num_waiters = 10;
    
    // 启动 10 个等待者
    std::cout << "  → 启动 " << num_waiters << " 个等待者...\n";
    for (int i = 0; i < num_waiters; ++i) {
        asio::co_spawn(ex, [event, i, &triggered_count]() -> asio::awaitable<void> {
            co_await event->wait(asio::use_awaitable);
            triggered_count.fetch_add(1, std::memory_order_relaxed);
            std::cout << "    等待者 " << i << " 被唤醒\n";
        }, asio::detached);
    }
    
    // 等待所有进入等待状态
    asio::steady_timer timer(ex, 100ms);
    co_await timer.async_wait(asio::use_awaitable);
    
    // Notify all
    std::cout << "  → 调用 notify_all()\n";
    event->notify_all();
    
    // 等待所有完成
    timer.expires_after(200ms);
    co_await timer.async_wait(asio::use_awaitable);
    
    if (triggered_count.load() == num_waiters) {
        std::cout << "  ✓ 所有 " << num_waiters << " 个等待者都被唤醒\n";
    } else {
        std::cout << "  ✗ 只有 " << triggered_count.load() << " 个被唤醒\n";
    }
    
    std::cout << "\n";
}

// 测试 3: notify_all() + reset() 的竞态
asio::awaitable<void> test_notify_reset_race() {
    auto ex = co_await asio::this_coro::executor;
    
    auto event = std::make_shared<acore::async_event>(ex);
    
    std::cout << "测试 3: notify_all() 和 reset() 竞态测试\n";
    std::cout << "  → 快速连续调用 notify_all() 和 reset()...\n";
    
    std::atomic<int> round1_triggered{0};
    std::atomic<int> round2_triggered{0};
    
    // 第一轮等待者
    for (int i = 0; i < 5; ++i) {
        asio::co_spawn(ex, [event, &round1_triggered]() -> asio::awaitable<void> {
            co_await event->wait(asio::use_awaitable);
            round1_triggered.fetch_add(1, std::memory_order_relaxed);
        }, asio::detached);
    }
    
    asio::steady_timer timer(ex, 50ms);
    co_await timer.async_wait(asio::use_awaitable);
    
    // 快速序列：notify → reset → 添加新waiter → notify
    event->notify_all();
    event->reset();
    
    // 等待 notify 和 reset 执行
    timer.expires_after(50ms);
    co_await timer.async_wait(asio::use_awaitable);
    
    // 第二轮等待者
    for (int i = 0; i < 5; ++i) {
        asio::co_spawn(ex, [event, &round2_triggered]() -> asio::awaitable<void> {
            co_await event->wait(asio::use_awaitable);
            round2_triggered.fetch_add(1, std::memory_order_relaxed);
        }, asio::detached);
    }
    
    timer.expires_after(50ms);
    co_await timer.async_wait(asio::use_awaitable);
    
    // 再次 notify
    event->notify_all();
    
    timer.expires_after(100ms);
    co_await timer.async_wait(asio::use_awaitable);
    
    std::cout << "  → 第一轮被唤醒: " << round1_triggered.load() << "\n";
    std::cout << "  → 第二轮被唤醒: " << round2_triggered.load() << "\n";
    
    if (round1_triggered.load() == 5) {
        std::cout << "  ✓ 第一轮正确（5个）\n";
    }
    
    if (round2_triggered.load() == 5) {
        std::cout << "  ✓ 第二轮正确（5个）\n";
    }
    
    std::cout << "\n";
}

// 测试 4: 重复 notify_all() 的幂等性
asio::awaitable<void> test_multiple_notify() {
    auto ex = co_await asio::this_coro::executor;
    
    auto event = std::make_shared<acore::async_event>(ex);
    
    std::cout << "测试 4: 重复 notify_all() 的幂等性\n";
    
    std::atomic<int> triggered{0};
    
    // 启动 3 个等待者
    for (int i = 0; i < 3; ++i) {
        asio::co_spawn(ex, [event, &triggered]() -> asio::awaitable<void> {
            co_await event->wait(asio::use_awaitable);
            triggered.fetch_add(1, std::memory_order_relaxed);
        }, asio::detached);
    }
    
    asio::steady_timer timer(ex, 50ms);
    co_await timer.async_wait(asio::use_awaitable);
    
    // 连续 notify 3 次
    std::cout << "  → 连续调用 notify_all() 3次\n";
    event->notify_all();
    event->notify_all();
    event->notify_all();
    
    timer.expires_after(100ms);
    co_await timer.async_wait(asio::use_awaitable);
    
    if (triggered.load() == 3) {
        std::cout << "  ✓ 每个等待者只被唤醒一次（幂等性正确）\n";
    } else {
        std::cout << "  ✗ 触发了 " << triggered.load() << " 次（应该是3）\n";
    }
    
    std::cout << "\n";
}

// 测试 5: 超时等待
asio::awaitable<void> test_wait_for_timeout() {
    auto ex = co_await asio::this_coro::executor;
    
    auto event = std::make_shared<acore::async_event>(ex);
    
    std::cout << "测试 5: 超时等待\n";
    
    // 超时测试
    std::cout << "  → 等待 200ms（不会 notify）...\n";
    bool result1 = co_await event->wait_for(200ms, asio::use_awaitable);
    
    if (!result1) {
        std::cout << "  ✓ 正确超时\n";
    } else {
        std::cout << "  ✗ 不应该被触发\n";
    }
    
    // 成功测试
    std::cout << "  → 100ms 后 notify，等待 500ms...\n";
    asio::steady_timer timer(ex, 100ms);
    timer.async_wait([event](const std::error_code&) {
        event->notify_all();
    });
    
    bool result2 = co_await event->wait_for(500ms, asio::use_awaitable);
    
    if (result2) {
        std::cout << "  ✓ 在超时前被正确触发\n";
    } else {
        std::cout << "  ✗ 不应该超时\n";
    }
    
    std::cout << "\n";
}

int main() {
    try {
        asio::io_context io_context;
        
        asio::co_spawn(io_context, []() -> asio::awaitable<void> {
            co_await test_basic_event();
            co_await test_broadcast();
            co_await test_notify_reset_race();
            co_await test_multiple_notify();
            co_await test_wait_for_timeout();
            
            std::cout << "=================================\n";
            std::cout << "async_event 所有测试完成！✓\n";
            std::cout << "=================================\n";
        }, asio::detached);
        
        io_context.run();
        
    } catch (const std::exception& e) {
        std::cerr << "异常: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}

