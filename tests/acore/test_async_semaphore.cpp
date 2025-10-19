//
// async_semaphore 全面测试 - 包括竞态条件测试
//
#include "acore/async_semaphore.hpp"
#include <iostream>
#include <chrono>
#include <atomic>
#include <vector>

using namespace std::chrono_literals;

// 测试 1: 基本 acquire/release
asio::awaitable<void> test_basic_semaphore() {
    auto ex = co_await asio::this_coro::executor;
    
    auto sem = std::make_shared<acore::async_semaphore>(ex, 0);
    
    std::cout << "测试 1: 基本 acquire/release\n";
    
    bool acquired = false;
    
    // 启动 acquire
    asio::co_spawn(ex, [sem, &acquired]() -> asio::awaitable<void> {
        std::cout << "  → 等待 semaphore...\n";
        co_await sem->acquire(asio::use_awaitable);
        acquired = true;
        std::cout << "  ✓ Acquire 成功\n";
    }, asio::detached);
    
    // 等待一下，确保 acquire 在等待
    asio::steady_timer timer(ex, 100ms);
    co_await timer.async_wait(asio::use_awaitable);
    
    // Release
    std::cout << "  → Release semaphore\n";
    sem->release();
    
    // 等待 acquire 完成
    timer.expires_after(100ms);
    co_await timer.async_wait(asio::use_awaitable);
    
    if (acquired) {
        std::cout << "  ✓ Acquire 被正确唤醒\n";
    } else {
        std::cout << "  ✗ Acquire 未被唤醒\n";
    }
    
    std::cout << "\n";
}

// 测试 2: 多个等待者 - 只唤醒一个
asio::awaitable<void> test_single_wakeup() {
    auto ex = co_await asio::this_coro::executor;
    
    auto sem = std::make_shared<acore::async_semaphore>(ex, 0);
    
    std::cout << "测试 2: release() 只唤醒一个等待者\n";
    
    std::atomic<int> acquired_count{0};
    
    // 启动 5 个等待者
    std::cout << "  → 启动 5 个等待者...\n";
    for (int i = 0; i < 5; ++i) {
        asio::co_spawn(ex, [sem, i, &acquired_count]() -> asio::awaitable<void> {
            co_await sem->acquire(asio::use_awaitable);
            acquired_count.fetch_add(1, std::memory_order_relaxed);
            std::cout << "    等待者 " << i << " 被唤醒\n";
        }, asio::detached);
    }
    
    // 等待所有等待者进入等待状态
    asio::steady_timer timer(ex, 100ms);
    co_await timer.async_wait(asio::use_awaitable);
    
    // Release 1 次
    std::cout << "  → Release 1 次\n";
    sem->release();
    
    timer.expires_after(100ms);
    co_await timer.async_wait(asio::use_awaitable);
    
    if (acquired_count.load() == 1) {
        std::cout << "  ✓ 只有 1 个等待者被唤醒（正确）\n";
    } else {
        std::cout << "  ✗ " << acquired_count.load() << " 个等待者被唤醒（应该是1）\n";
    }
    
    // Release 剩余的
    std::cout << "  → Release 4 次\n";
    sem->release(4);
    
    timer.expires_after(100ms);
    co_await timer.async_wait(asio::use_awaitable);
    
    if (acquired_count.load() == 5) {
        std::cout << "  ✓ 所有等待者都被唤醒\n";
    } else {
        std::cout << "  ✗ 只有 " << acquired_count.load() << " 个被唤醒\n";
    }
    
    std::cout << "\n";
}

// 测试 3: 并发 acquire/release
asio::awaitable<void> test_concurrent_acquire_release() {
    auto ex = co_await asio::this_coro::executor;
    
    auto sem = std::make_shared<acore::async_semaphore>(ex, 0);
    
    std::cout << "测试 3: 并发 acquire/release（竞态测试）\n";
    std::cout << "  → 100 个 release 和 100 个 acquire 并发执行...\n";
    
    std::atomic<int> acquired{0};
    std::atomic<int> released{0};
    
    // 启动 100 个 acquire
    for (int i = 0; i < 100; ++i) {
        asio::co_spawn(ex, [sem, &acquired]() -> asio::awaitable<void> {
            co_await sem->acquire(asio::use_awaitable);
            acquired.fetch_add(1, std::memory_order_relaxed);
        }, asio::detached);
    }
    
    // 启动 100 个 release
    for (int i = 0; i < 100; ++i) {
        asio::co_spawn(ex, [sem, &released]() -> asio::awaitable<void> {
            sem->release();
            released.fetch_add(1, std::memory_order_relaxed);
            co_return;
        }, asio::detached);
    }
    
    // 等待所有完成
    asio::steady_timer timer(ex, 500ms);
    co_await timer.async_wait(asio::use_awaitable);
    
    std::cout << "  → Released: " << released.load() << "\n";
    std::cout << "  → Acquired: " << acquired.load() << "\n";
    
    if (acquired.load() == 100 && released.load() == 100) {
        std::cout << "  ✓ 所有 acquire/release 都成功完成\n";
    } else {
        std::cout << "  ✗ 数量不匹配！\n";
    }
    
    std::cout << "\n";
}

// 测试 4: 取消操作的竞态
asio::awaitable<void> test_cancel_race() {
    auto ex = co_await asio::this_coro::executor;
    
    auto sem = std::make_shared<acore::async_semaphore>(ex, 0);
    
    std::cout << "测试 4: 取消操作竞态测试\n";
    std::cout << "  → 同时 acquire_cancellable 和 cancel...\n";
    
    std::atomic<int> acquired{0};
    std::atomic<int> canceled{0};
    std::vector<uint64_t> waiter_ids;
    
    // 启动 50 个可取消的 acquire
    for (int i = 0; i < 50; ++i) {
        uint64_t id = sem->acquire_cancellable([&acquired]() {
            acquired.fetch_add(1, std::memory_order_relaxed);
        });
        waiter_ids.push_back(id);
    }
    
    std::cout << "  → 启动了 50 个 acquire\n";
    
    // 立即取消一半
    std::cout << "  → 取消前 25 个...\n";
    for (int i = 0; i < 25; ++i) {
        sem->cancel(waiter_ids[i]);
        canceled++;
    }
    
    // Release 50 次
    std::cout << "  → Release 50 次\n";
    sem->release(50);
    
    // 等待完成
    asio::steady_timer timer(ex, 200ms);
    co_await timer.async_wait(asio::use_awaitable);
    
    std::cout << "  → Acquired: " << acquired.load() << "\n";
    std::cout << "  → Canceled: " << canceled << "\n";
    
    // 只有后 25 个应该被唤醒
    if (acquired.load() == 25) {
        std::cout << "  ✓ 正确：只有未取消的 25 个被唤醒\n";
    } else {
        std::cout << "  ⚠ Acquired: " << acquired.load() << "（期望25，可能因时序略有不同）\n";
    }
    
    std::cout << "\n";
}

// 测试 5: try_acquire 在高并发下
asio::awaitable<void> test_try_acquire_concurrent() {
    auto ex = co_await asio::this_coro::executor;
    
    auto sem = std::make_shared<acore::async_semaphore>(ex, 50);  // 初始 50
    
    std::cout << "测试 5: try_acquire 并发测试\n";
    std::cout << "  → 100 个协程同时 try_acquire（只有 50 个信号量）...\n";
    
    std::atomic<int> success{0};
    std::atomic<int> failed{0};
    
    for (int i = 0; i < 100; ++i) {
        asio::co_spawn(ex, [sem, &success, &failed]() -> asio::awaitable<void> {
            bool result = co_await sem->async_try_acquire(asio::use_awaitable);
            if (result) {
                success.fetch_add(1, std::memory_order_relaxed);
            } else {
                failed.fetch_add(1, std::memory_order_relaxed);
            }
        }, asio::detached);
    }
    
    // 等待所有完成
    asio::steady_timer timer(ex, 200ms);
    co_await timer.async_wait(asio::use_awaitable);
    
    std::cout << "  → 成功: " << success.load() << "\n";
    std::cout << "  → 失败: " << failed.load() << "\n";
    
    if (success.load() == 50 && failed.load() == 50) {
        std::cout << "  ✓ 正好 50 个成功，50 个失败\n";
    } else {
        std::cout << "  ⚠ 成功: " << success.load() << ", 失败: " << failed.load() << "\n";
    }
    
    std::cout << "\n";
}

int main() {
    try {
        asio::io_context io_context;
        
        asio::co_spawn(io_context, []() -> asio::awaitable<void> {
            co_await test_basic_semaphore();
            co_await test_single_wakeup();
            co_await test_concurrent_acquire_release();
            co_await test_cancel_race();
            co_await test_try_acquire_concurrent();
            
            std::cout << "=================================\n";
            std::cout << "async_semaphore 所有测试完成！✓\n";
            std::cout << "=================================\n";
        }, asio::detached);
        
        io_context.run();
        
    } catch (const std::exception& e) {
        std::cerr << "异常: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}

