//
// Test async_semaphore implementation
//

#include "acore/async_semaphore.hpp"
#include <asio.hpp>
#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/use_awaitable.hpp>
#include <iostream>
#include <chrono>

using asio::awaitable;
using asio::co_spawn;
using asio::detached;
using asio::use_awaitable;
using namespace std::chrono_literals;

// 测试1：基本的 acquire 和 release
awaitable<void> test_basic(asio::io_context& io) {
    std::cout << "\n=== Test 1: Basic acquire/release ===" << std::endl;
    
    acore::async_semaphore sem(io.get_executor(), 0);
    
    // 启动一个等待者
    co_spawn(io, [&sem]() -> awaitable<void> {
        std::cout << "[Waiter] Waiting for semaphore..." << std::endl;
        co_await sem.acquire();
        std::cout << "[Waiter] Acquired! ✅" << std::endl;
    }, detached);
    
    // 延迟释放
    auto executor = co_await asio::this_coro::executor;
    asio::steady_timer timer(executor);
    timer.expires_after(100ms);
    co_await timer.async_wait(use_awaitable);
    
    std::cout << "[Main] Releasing semaphore..." << std::endl;
    sem.release();
    
    // 等待完成
    timer.expires_after(100ms);
    co_await timer.async_wait(use_awaitable);
}

// 测试2：多个等待者，只唤醒一个
awaitable<void> test_single_wakeup(asio::io_context& io) {
    std::cout << "\n=== Test 2: Single wakeup (1 release, 3 waiters) ===" << std::endl;
    
    acore::async_semaphore sem(io.get_executor(), 0);
    
    // 启动3个等待者
    for (int i = 1; i <= 3; ++i) {
        co_spawn(io, [&sem, i]() -> awaitable<void> {
            std::cout << "[Waiter " << i << "] Waiting..." << std::endl;
            co_await sem.acquire();
            std::cout << "[Waiter " << i << "] Acquired! ✅" << std::endl;
        }, detached);
    }
    
    auto executor = co_await asio::this_coro::executor;
    asio::steady_timer timer(executor);
    timer.expires_after(100ms);
    co_await timer.async_wait(use_awaitable);
    
    // 只释放一次
    std::cout << "[Main] Releasing once..." << std::endl;
    sem.release();
    
    timer.expires_after(200ms);
    co_await timer.async_wait(use_awaitable);
    
    std::cout << "[Main] Expected: Only 1 waiter acquired" << std::endl;
}

// 测试3：初始计数 > 0
awaitable<void> test_initial_count(asio::io_context& io) {
    std::cout << "\n=== Test 3: Initial count > 0 ===" << std::endl;
    
    acore::async_semaphore sem(io.get_executor(), 2);
    
    std::cout << "[Main] Semaphore created with count=2" << std::endl;
    std::cout << "[Main] Current count: " << sem.count() << std::endl;
    
    // 应该能立即获取两次
    co_spawn(io, [&sem]() -> awaitable<void> {
        std::cout << "[Waiter 1] Acquiring..." << std::endl;
        co_await sem.acquire();
        std::cout << "[Waiter 1] Acquired immediately! ✅" << std::endl;
    }, detached);
    
    co_spawn(io, [&sem]() -> awaitable<void> {
        std::cout << "[Waiter 2] Acquiring..." << std::endl;
        co_await sem.acquire();
        std::cout << "[Waiter 2] Acquired immediately! ✅" << std::endl;
    }, detached);
    
    // 第三个应该等待
    co_spawn(io, [&sem]() -> awaitable<void> {
        std::cout << "[Waiter 3] Acquiring..." << std::endl;
        co_await sem.acquire();
        std::cout << "[Waiter 3] Acquired after wait ✅" << std::endl;
    }, detached);
    
    auto executor = co_await asio::this_coro::executor;
    asio::steady_timer timer(executor);
    timer.expires_after(100ms);
    co_await timer.async_wait(use_awaitable);
    
    std::cout << "[Main] Releasing for waiter 3..." << std::endl;
    sem.release();
    
    timer.expires_after(100ms);
    co_await timer.async_wait(use_awaitable);
}

// 测试4：批量释放
awaitable<void> test_batch_release(asio::io_context& io) {
    std::cout << "\n=== Test 4: Batch release ===" << std::endl;
    
    acore::async_semaphore sem(io.get_executor(), 0);
    
    // 启动5个等待者
    for (int i = 1; i <= 5; ++i) {
        co_spawn(io, [&sem, i]() -> awaitable<void> {
            std::cout << "[Waiter " << i << "] Waiting..." << std::endl;
            co_await sem.acquire();
            std::cout << "[Waiter " << i << "] Acquired! ✅" << std::endl;
        }, detached);
    }
    
    auto executor = co_await asio::this_coro::executor;
    asio::steady_timer timer(executor);
    timer.expires_after(100ms);
    co_await timer.async_wait(use_awaitable);
    
    // 批量释放3个
    std::cout << "[Main] Batch releasing 3..." << std::endl;
    sem.release(3);
    
    timer.expires_after(200ms);
    co_await timer.async_wait(use_awaitable);
    
    std::cout << "[Main] Expected: 3 waiters acquired, 2 still waiting" << std::endl;
}

// 测试5：try_acquire
awaitable<void> test_try_acquire(asio::io_context& io) {
    std::cout << "\n=== Test 5: try_acquire ===" << std::endl;
    
    acore::async_semaphore sem(io.get_executor(), 1);
    
    std::cout << "[Main] Initial count: " << sem.count() << std::endl;
    
    // 应该成功
    bool success1 = co_await sem.try_acquire();
    std::cout << "[Main] First try_acquire: " << (success1 ? "SUCCESS ✅" : "FAILED ❌") << std::endl;
    std::cout << "[Main] Count after: " << sem.count() << std::endl;
    
    // 应该失败（计数为0）
    bool success2 = co_await sem.try_acquire();
    std::cout << "[Main] Second try_acquire: " << (success2 ? "SUCCESS ❌" : "FAILED ✅ (expected)") << std::endl;
    std::cout << "[Main] Count after: " << sem.count() << std::endl;
}

// 测试6：压力测试 - 生产者消费者
awaitable<void> test_producer_consumer(asio::io_context& io) {
    std::cout << "\n=== Test 6: Producer-Consumer stress test ===" << std::endl;
    
    acore::async_semaphore sem(io.get_executor(), 0);
    auto consumed = std::make_shared<std::atomic<int>>(0);
    
    // 10个消费者
    for (int i = 1; i <= 10; ++i) {
        co_spawn(io, [&sem, consumed, i]() -> awaitable<void> {
            for (int j = 0; j < 10; ++j) {
                co_await sem.acquire();
                consumed->fetch_add(1);
            }
            std::cout << "[Consumer " << i << "] Finished 10 items" << std::endl;
        }, detached);
    }
    
    // 生产者：快速释放100次
    std::cout << "[Main] Producing 100 items..." << std::endl;
    for (int i = 0; i < 100; ++i) {
        sem.release();
    }
    
    // 等待所有消费完成
    auto executor = co_await asio::this_coro::executor;
    asio::steady_timer timer(executor);
    timer.expires_after(500ms);
    co_await timer.async_wait(use_awaitable);
    
    std::cout << "[Main] Total consumed: " << consumed->load() 
              << " (expected: 100)" << std::endl;
    
    if (consumed->load() == 100) {
        std::cout << "✅ Producer-Consumer test PASSED" << std::endl;
    } else {
        std::cout << "❌ Producer-Consumer test FAILED" << std::endl;
    }
}

int main() {
    try {
        std::cout << "Async Semaphore Test Suite" << std::endl;
        std::cout << "===========================" << std::endl;
        
        asio::io_context io;
        
        // 运行所有测试
        co_spawn(io, [&io]() -> awaitable<void> {
            co_await test_basic(io);
            co_await test_single_wakeup(io);
            co_await test_initial_count(io);
            co_await test_batch_release(io);
            co_await test_try_acquire(io);
            co_await test_producer_consumer(io);
            
            std::cout << "\n===========================" << std::endl;
            std::cout << "All tests completed!" << std::endl;
        }, detached);
        
        io.run();
        
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}


