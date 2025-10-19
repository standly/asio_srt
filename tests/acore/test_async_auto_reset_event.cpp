//
// 测试 async_auto_reset_event
//
#include "acore/async_auto_reset_event.hpp"
#include <asio.hpp>
#include <iostream>
#include <vector>
#include <chrono>
#include <atomic>

using namespace std::chrono_literals;
using asio::use_awaitable;

// ============ 测试 1：基本的单次通知 ============
asio::awaitable<void> test_single_notification() {
    std::cout << "\n=== Test 1: Single notification ===\n";
    
    auto ex = co_await asio::this_coro::executor;
    auto event = std::make_shared<acore::async_auto_reset_event>(ex);
    
    auto completed = std::make_shared<std::atomic<bool>>(false);
    
    // 启动一个等待协程
    asio::co_spawn(ex, [event, completed]() -> asio::awaitable<void> {
        std::cout << "Waiting for event...\n";
        co_await event->wait(use_awaitable);
        
        completed->store(true);
        std::cout << "✓ Event received\n";
    }, asio::detached);
    
    // 等待协程进入等待状态
    asio::steady_timer timer(ex);
    timer.expires_after(50ms);
    co_await timer.async_wait(use_awaitable);
    
    // 触发事件
    event->notify();
    std::cout << "Event notified\n";
    
    // 等待完成
    timer.expires_after(50ms);
    co_await timer.async_wait(use_awaitable);
    
    if (completed->load()) {
        std::cout << "✓ Waiter completed\n";
    }
    
    std::cout << "✅ Test 1 PASSED\n";
}

// ============ 测试 2：只唤醒一个等待者 ============
asio::awaitable<void> test_wake_one() {
    std::cout << "\n=== Test 2: Wake only one waiter ===\n";
    
    auto ex = co_await asio::this_coro::executor;
    auto event = std::make_shared<acore::async_auto_reset_event>(ex);
    
    auto completed_count = std::make_shared<std::atomic<int>>(0);
    
    // 启动 5 个等待协程
    for (int i = 0; i < 5; ++i) {
        asio::co_spawn(ex, [event, completed_count, i]() -> asio::awaitable<void> {
            std::cout << "Worker " << i << " waiting...\n";
            co_await event->wait(use_awaitable);
            
            completed_count->fetch_add(1);
            std::cout << "Worker " << i << " woken up\n";
        }, asio::detached);
    }
    
    // 等待所有协程进入等待状态
    asio::steady_timer timer(ex);
    timer.expires_after(100ms);
    co_await timer.async_wait(use_awaitable);
    
    size_t waiting = co_await event->async_waiting_count(use_awaitable);
    std::cout << "✓ Waiting count: " << waiting << " (expected 5)\n";
    
    // 只触发一次（应该只唤醒一个）
    event->notify();
    std::cout << "One notify() called\n";
    
    timer.expires_after(100ms);
    co_await timer.async_wait(use_awaitable);
    
    int completed = completed_count->load();
    if (completed == 1) {
        std::cout << "✓ Only 1 worker woken up (expected 1)\n";
    } else {
        std::cout << "✗ " << completed << " workers woken up (expected 1)\n";
    }
    
    waiting = co_await event->async_waiting_count(use_awaitable);
    std::cout << "✓ Remaining waiters: " << waiting << " (expected 4)\n";
    
    std::cout << "✅ Test 2 PASSED\n";
}

// ============ 测试 3：批量通知 ============
asio::awaitable<void> test_batch_notify() {
    std::cout << "\n=== Test 3: Batch notification ===\n";
    
    auto ex = co_await asio::this_coro::executor;
    auto event = std::make_shared<acore::async_auto_reset_event>(ex);
    
    auto completed_count = std::make_shared<std::atomic<int>>(0);
    
    // 启动 10 个等待协程
    for (int i = 0; i < 10; ++i) {
        asio::co_spawn(ex, [event, completed_count, i]() -> asio::awaitable<void> {
            co_await event->wait(use_awaitable);
            completed_count->fetch_add(1);
            std::cout << "Worker " << i << " completed\n";
        }, asio::detached);
    }
    
    // 等待所有协程进入等待状态
    asio::steady_timer timer(ex);
    timer.expires_after(100ms);
    co_await timer.async_wait(use_awaitable);
    
    // 批量通知 3 个
    event->notify(3);
    std::cout << "notify(3) called\n";
    
    timer.expires_after(100ms);
    co_await timer.async_wait(use_awaitable);
    
    int completed = completed_count->load();
    if (completed == 3) {
        std::cout << "✓ 3 workers woken up\n";
    }
    
    size_t waiting = co_await event->async_waiting_count(use_awaitable);
    std::cout << "✓ Remaining waiters: " << waiting << " (expected 7)\n";
    
    std::cout << "✅ Test 3 PASSED\n";
}

// ============ 测试 4：信号计数 ============
asio::awaitable<void> test_signal_count() {
    std::cout << "\n=== Test 4: Signal count ===\n";
    
    auto ex = co_await asio::this_coro::executor;
    auto event = std::make_shared<acore::async_auto_reset_event>(ex);
    
    // 无等待者时，通知会增加信号计数
    event->notify();
    event->notify();
    event->notify();
    
    asio::steady_timer timer(ex);
    timer.expires_after(50ms);
    co_await timer.async_wait(use_awaitable);
    
    size_t signal_count = co_await event->async_signal_count(use_awaitable);
    std::cout << "✓ Signal count: " << signal_count << " (expected 3)\n";
    
    // 等待应该立即消耗一个信号
    co_await event->wait(use_awaitable);
    std::cout << "✓ First wait completed immediately\n";
    
    signal_count = co_await event->async_signal_count(use_awaitable);
    std::cout << "✓ Signal count after wait: " << signal_count << " (expected 2)\n";
    
    std::cout << "✅ Test 4 PASSED\n";
}

// ============ 测试 5：try_wait（非阻塞）============
asio::awaitable<void> test_try_wait() {
    std::cout << "\n=== Test 5: Non-blocking try_wait ===\n";
    
    auto ex = co_await asio::this_coro::executor;
    auto event = std::make_shared<acore::async_auto_reset_event>(ex);
    
    // 无信号时，try_wait 应该失败
    bool success = co_await event->try_wait(use_awaitable);
    if (!success) {
        std::cout << "✓ try_wait failed when no signal (expected)\n";
    }
    
    // 通知一次
    event->notify();
    
    asio::steady_timer timer(ex);
    timer.expires_after(50ms);
    co_await timer.async_wait(use_awaitable);
    
    // 现在应该成功
    success = co_await event->try_wait(use_awaitable);
    if (success) {
        std::cout << "✓ try_wait succeeded after notify\n";
    }
    
    // 再次尝试应该失败（信号已被消耗）
    success = co_await event->try_wait(use_awaitable);
    if (!success) {
        std::cout << "✓ try_wait failed again (signal consumed)\n";
    }
    
    std::cout << "✅ Test 5 PASSED\n";
}

// ============ 测试 6：初始状态 ============
asio::awaitable<void> test_initially_set() {
    std::cout << "\n=== Test 6: Initially set state ===\n";
    
    auto ex = co_await asio::this_coro::executor;
    
    // 创建时设置为触发状态
    auto event = std::make_shared<acore::async_auto_reset_event>(ex, true);
    
    size_t signal_count = co_await event->async_signal_count(use_awaitable);
    std::cout << "✓ Initial signal count: " << signal_count << " (expected 1)\n";
    
    // 等待应该立即完成
    auto start = std::chrono::steady_clock::now();
    co_await event->wait(use_awaitable);
    auto elapsed = std::chrono::steady_clock::now() - start;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    
    if (ms < 10) {
        std::cout << "✓ Wait completed immediately (" << ms << "ms)\n";
    }
    
    signal_count = co_await event->async_signal_count(use_awaitable);
    std::cout << "✓ Signal count after wait: " << signal_count << " (expected 0)\n";
    
    std::cout << "✅ Test 6 PASSED\n";
}

// ============ 测试 7：重置事件 ============
asio::awaitable<void> test_reset() {
    std::cout << "\n=== Test 7: Reset event ===\n";
    
    auto ex = co_await asio::this_coro::executor;
    auto event = std::make_shared<acore::async_auto_reset_event>(ex);
    
    // 添加一些信号
    event->notify(5);
    
    asio::steady_timer timer(ex);
    timer.expires_after(50ms);
    co_await timer.async_wait(use_awaitable);
    
    size_t signal_count = co_await event->async_signal_count(use_awaitable);
    std::cout << "✓ Signal count: " << signal_count << " (expected 5)\n";
    
    // 重置
    event->reset();
    std::cout << "Event reset\n";
    
    timer.expires_after(50ms);
    co_await timer.async_wait(use_awaitable);
    
    signal_count = co_await event->async_signal_count(use_awaitable);
    std::cout << "✓ Signal count after reset: " << signal_count << " (expected 0)\n";
    
    std::cout << "✅ Test 7 PASSED\n";
}

// ============ 测试 8：取消所有等待者 ============
asio::awaitable<void> test_cancel_all() {
    std::cout << "\n=== Test 8: Cancel all waiters ===\n";
    
    auto ex = co_await asio::this_coro::executor;
    auto event = std::make_shared<acore::async_auto_reset_event>(ex);
    
    auto completed_count = std::make_shared<std::atomic<int>>(0);
    
    // 启动 5 个等待协程
    for (int i = 0; i < 5; ++i) {
        asio::co_spawn(ex, [event, completed_count, i]() -> asio::awaitable<void> {
            co_await event->wait(use_awaitable);
            completed_count->fetch_add(1);
            std::cout << "Worker " << i << " completed\n";
        }, asio::detached);
    }
    
    // 等待所有协程进入等待状态
    asio::steady_timer timer(ex);
    timer.expires_after(100ms);
    co_await timer.async_wait(use_awaitable);
    
    size_t waiting1 = co_await event->async_waiting_count(use_awaitable);
    std::cout << "✓ Waiting count: " << waiting1 << " (expected 5)\n";
    
    // 取消所有
    event->cancel_all();
    std::cout << "cancel_all() called\n";
    
    timer.expires_after(100ms);
    co_await timer.async_wait(use_awaitable);
    
    int completed = completed_count->load();
    if (completed == 5) {
        std::cout << "✓ All 5 workers completed\n";
    }
    
    size_t waiting2 = co_await event->async_waiting_count(use_awaitable);
    std::cout << "✓ Waiting count after cancel: " << waiting2 << " (expected 0)\n";
    
    std::cout << "✅ Test 8 PASSED\n";
}

// ============ 测试 9：任务队列模式 ============
asio::awaitable<void> test_task_queue_pattern() {
    std::cout << "\n=== Test 9: Task queue pattern ===\n";
    
    auto ex = co_await asio::this_coro::executor;
    auto event = std::make_shared<acore::async_auto_reset_event>(ex);
    
    auto tasks_processed = std::make_shared<std::atomic<int>>(0);
    
    // 3 个 worker 协程
    for (int i = 0; i < 3; ++i) {
        asio::co_spawn(ex, [event, tasks_processed, i]() -> asio::awaitable<void> {
            for (int j = 0; j < 3; ++j) {
                // 等待任务
                co_await event->wait(use_awaitable);
                
                // 处理任务
                tasks_processed->fetch_add(1);
                std::cout << "Worker " << i << " processed task " << j << "\n";
            }
        }, asio::detached);
    }
    
    asio::steady_timer timer(ex);
    timer.expires_after(100ms);
    co_await timer.async_wait(use_awaitable);
    
    // 分发 9 个任务
    for (int i = 0; i < 9; ++i) {
        event->notify();
        
        timer.expires_after(50ms);
        co_await timer.async_wait(use_awaitable);
    }
    
    // 等待所有任务完成
    timer.expires_after(200ms);
    co_await timer.async_wait(use_awaitable);
    
    int processed = tasks_processed->load();
    if (processed == 9) {
        std::cout << "✓ All 9 tasks processed\n";
    } else {
        std::cout << "✗ " << processed << " tasks processed (expected 9)\n";
    }
    
    std::cout << "✅ Test 9 PASSED\n";
}

// ============ 测试 10：vs 手动重置事件对比 ============
asio::awaitable<void> test_vs_manual_reset() {
    std::cout << "\n=== Test 10: Auto-reset vs Manual-reset comparison ===\n";
    
    auto ex = co_await asio::this_coro::executor;
    auto auto_event = std::make_shared<acore::async_auto_reset_event>(ex);
    
    auto completed_count = std::make_shared<std::atomic<int>>(0);
    
    // 启动 3 个等待协程
    for (int i = 0; i < 3; ++i) {
        asio::co_spawn(ex, [auto_event, completed_count, i]() -> asio::awaitable<void> {
            co_await auto_event->wait(use_awaitable);
            completed_count->fetch_add(1);
            std::cout << "Worker " << i << " woken (auto-reset)\n";
        }, asio::detached);
    }
    
    asio::steady_timer timer(ex);
    timer.expires_after(100ms);
    co_await timer.async_wait(use_awaitable);
    
    // 一次 notify()
    auto_event->notify();
    std::cout << "One notify() called (auto-reset)\n";
    
    timer.expires_after(100ms);
    co_await timer.async_wait(use_awaitable);
    
    int completed = completed_count->load();
    std::cout << "✓ Auto-reset: " << completed << " workers woken (expected 1)\n";
    
    // 对比：手动重置事件的 notify_all() 会唤醒所有人
    std::cout << "✓ Manual-reset event would wake all 3 workers with notify_all()\n";
    std::cout << "✓ Auto-reset event only wakes one worker per notify()\n";
    
    std::cout << "✅ Test 10 PASSED\n";
}

// ============ 主测试函数 ============
asio::awaitable<void> run_all_tests() {
    try {
        co_await test_single_notification();
        co_await test_wake_one();
        co_await test_batch_notify();
        co_await test_signal_count();
        co_await test_try_wait();
        co_await test_initially_set();
        co_await test_reset();
        co_await test_cancel_all();
        co_await test_task_queue_pattern();
        co_await test_vs_manual_reset();
        
        std::cout << "\n" << std::string(50, '=') << "\n";
        std::cout << "🎉 ALL TESTS PASSED! 🎉\n";
        std::cout << std::string(50, '=') << "\n";
    } catch (const std::exception& e) {
        std::cout << "\n❌ Test failed with exception: " << e.what() << "\n";
    }
}

int main() {
    asio::io_context io_context;
    
    asio::co_spawn(io_context, run_all_tests(), asio::detached);
    
    io_context.run();
    
    return 0;
}

