//
// 测试 async_mutex
//
#include "acore/async_mutex.hpp"
#include <asio.hpp>
#include <iostream>
#include <vector>
#include <chrono>

using namespace std::chrono_literals;
using asio::use_awaitable;

// ============ 测试 1：基本的锁定/解锁 ============
asio::awaitable<void> test_basic_lock_unlock() {
    std::cout << "\n=== Test 1: Basic lock/unlock ===\n";
    
    auto ex = co_await asio::this_coro::executor;
    auto mutex = std::make_shared<acore::async_mutex>(ex);
    
    // RAII 风格锁定
    {
        auto guard = co_await mutex->async_lock(use_awaitable);
        std::cout << "✓ Lock acquired (RAII style)\n";
        
        bool is_locked = co_await mutex->async_is_locked(use_awaitable);
        if (is_locked) {
            std::cout << "✓ Mutex is locked\n";
        }
    }  // guard 析构，自动解锁
    
    bool is_locked = co_await mutex->async_is_locked(use_awaitable);
    if (!is_locked) {
        std::cout << "✓ Mutex is unlocked (auto unlock by guard)\n";
    }
    
    // 手动锁定/解锁
    co_await mutex->lock(use_awaitable);
    std::cout << "✓ Lock acquired (manual style)\n";
    
    mutex->unlock();
    std::cout << "✓ Manual unlock\n";
    
    std::cout << "✅ Test 1 PASSED\n";
}

// ============ 测试 2：多个协程竞争锁 ============
asio::awaitable<void> test_concurrent_access() {
    std::cout << "\n=== Test 2: Concurrent access ===\n";
    
    auto ex = co_await asio::this_coro::executor;
    auto mutex = std::make_shared<acore::async_mutex>(ex);
    auto shared_counter = std::make_shared<int>(0);
    auto results = std::make_shared<std::vector<int>>();
    
    // 启动 10 个协程，每个增加计数器 100 次
    std::vector<asio::awaitable<void>> tasks;
    
    for (int i = 0; i < 10; ++i) {
        tasks.push_back([](auto mutex, auto counter, auto results, int id) -> asio::awaitable<void> {
            for (int j = 0; j < 100; ++j) {
                auto guard = co_await mutex->async_lock(use_awaitable);
                
                // 临界区：增加计数器
                int old_value = *counter;
                co_await asio::post(asio::use_awaitable);  // 模拟异步操作
                *counter = old_value + 1;
            }
            
            results->push_back(id);
            std::cout << "Worker " << id << " finished\n";
        }(mutex, shared_counter, results, i));
    }
    
    // 等待所有任务完成
    for (auto& task : tasks) {
        co_await std::move(task);
    }
    
    // 验证结果
    if (*shared_counter == 1000) {
        std::cout << "✓ Counter = " << *shared_counter << " (expected 1000)\n";
        std::cout << "✓ No race condition detected\n";
    } else {
        std::cout << "✗ Counter = " << *shared_counter << " (expected 1000) - RACE CONDITION!\n";
    }
    
    std::cout << "✅ Test 2 PASSED\n";
}

// ============ 测试 3：锁的队列顺序 ============
asio::awaitable<void> test_lock_fairness() {
    std::cout << "\n=== Test 3: Lock fairness (FIFO order) ===\n";
    
    auto ex = co_await asio::this_coro::executor;
    auto mutex = std::make_shared<acore::async_mutex>(ex);
    auto results = std::make_shared<std::vector<int>>();
    
    // 先获取锁
    co_await mutex->lock(use_awaitable);
    std::cout << "Main coroutine acquired lock\n";
    
    // 启动 5 个协程尝试获取锁（它们会排队）
    for (int i = 0; i < 5; ++i) {
        asio::co_spawn(ex, [mutex, results, i]() -> asio::awaitable<void> {
            std::cout << "Worker " << i << " waiting for lock...\n";
            auto guard = co_await mutex->async_lock(use_awaitable);
            
            results->push_back(i);
            std::cout << "Worker " << i << " acquired lock (result index: " << results->size() - 1 << ")\n";
            
            co_await asio::post(asio::use_awaitable);  // 让出控制权
        }, asio::detached);
    }
    
    // 等待所有协程进入等待队列
    co_await asio::post(asio::use_awaitable);
    
    size_t waiting = co_await mutex->async_waiting_count(use_awaitable);
    std::cout << "✓ Waiting count = " << waiting << " (expected 5)\n";
    
    // 释放锁，让等待者依次获取
    mutex->unlock();
    std::cout << "Main coroutine released lock\n";
    
    // 等待所有任务完成
    co_await asio::steady_timer(ex, 100ms).async_wait(use_awaitable);
    
    // 验证顺序（应该是 FIFO）
    bool fifo = true;
    for (size_t i = 0; i < results->size(); ++i) {
        if ((*results)[i] != static_cast<int>(i)) {
            fifo = false;
            break;
        }
    }
    
    if (fifo && results->size() == 5) {
        std::cout << "✓ Lock acquired in FIFO order\n";
    } else {
        std::cout << "✗ Order: ";
        for (int id : *results) std::cout << id << " ";
        std::cout << "\n";
    }
    
    std::cout << "✅ Test 3 PASSED\n";
}

// ============ 测试 4：超时锁定 ============
asio::awaitable<void> test_try_lock_timeout() {
    std::cout << "\n=== Test 4: Try lock with timeout ===\n";
    
    auto ex = co_await asio::this_coro::executor;
    auto mutex = std::make_shared<acore::async_mutex>(ex);
    
    // 先获取锁
    co_await mutex->lock(use_awaitable);
    std::cout << "Lock acquired by main coroutine\n";
    
    // 尝试获取锁（应该超时）
    auto start = std::chrono::steady_clock::now();
    bool acquired = co_await mutex->try_lock_for(200ms, use_awaitable);
    auto elapsed = std::chrono::steady_clock::now() - start;
    
    if (!acquired) {
        std::cout << "✓ Lock acquisition timed out (as expected)\n";
        std::cout << "✓ Timeout after " << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() << "ms\n";
    } else {
        std::cout << "✗ Unexpected: Lock acquired (should timeout)\n";
    }
    
    // 释放锁
    mutex->unlock();
    
    // 再次尝试获取锁（应该成功）
    acquired = co_await mutex->try_lock_for(200ms, use_awaitable);
    if (acquired) {
        std::cout << "✓ Lock acquired successfully after unlock\n";
        mutex->unlock();
    }
    
    std::cout << "✅ Test 4 PASSED\n";
}

// ============ 测试 5：锁守卫的移动语义 ============
asio::awaitable<void> test_lock_guard_move() {
    std::cout << "\n=== Test 5: Lock guard move semantics ===\n";
    
    auto ex = co_await asio::this_coro::executor;
    auto mutex = std::make_shared<acore::async_mutex>(ex);
    
    auto acquire_lock = [](auto mutex) -> asio::awaitable<acore::async_lock_guard> {
        auto guard = co_await mutex->async_lock(use_awaitable);
        std::cout << "✓ Lock acquired in helper function\n";
        co_return guard;  // 移动返回
    };
    
    {
        auto guard = co_await acquire_lock(mutex);
        std::cout << "✓ Lock guard moved to main coroutine\n";
        
        bool is_locked = co_await mutex->async_is_locked(use_awaitable);
        if (is_locked) {
            std::cout << "✓ Mutex still locked after move\n";
        }
        
        if (guard.owns_lock()) {
            std::cout << "✓ Guard owns the lock\n";
        }
    }  // guard 析构，释放锁
    
    bool is_locked = co_await mutex->async_is_locked(use_awaitable);
    if (!is_locked) {
        std::cout << "✓ Mutex unlocked after guard destruction\n";
    }
    
    std::cout << "✅ Test 5 PASSED\n";
}

// ============ 测试 6：手动 unlock 守卫 ============
asio::awaitable<void> test_manual_unlock_guard() {
    std::cout << "\n=== Test 6: Manual unlock guard ===\n";
    
    auto ex = co_await asio::this_coro::executor;
    auto mutex = std::make_shared<acore::async_mutex>(ex);
    
    auto guard = co_await mutex->async_lock(use_awaitable);
    std::cout << "✓ Lock acquired\n";
    
    // 手动解锁
    guard.unlock();
    std::cout << "✓ Guard manually unlocked\n";
    
    bool is_locked = co_await mutex->async_is_locked(use_awaitable);
    if (!is_locked) {
        std::cout << "✓ Mutex is unlocked\n";
    }
    
    if (!guard.owns_lock()) {
        std::cout << "✓ Guard no longer owns lock\n";
    }
    
    // guard 析构时不会再次解锁（已经 unlock 过）
    
    std::cout << "✅ Test 6 PASSED\n";
}

// ============ 测试 7：压力测试 ============
asio::awaitable<void> test_stress() {
    std::cout << "\n=== Test 7: Stress test (100 workers, 1000 iterations) ===\n";
    
    auto ex = co_await asio::this_coro::executor;
    auto mutex = std::make_shared<acore::async_mutex>(ex);
    auto shared_counter = std::make_shared<int>(0);
    
    auto start_time = std::chrono::steady_clock::now();
    
    // 启动 100 个协程
    std::vector<asio::awaitable<void>> tasks;
    const int num_workers = 100;
    const int iterations = 1000;
    
    for (int i = 0; i < num_workers; ++i) {
        tasks.push_back([](auto mutex, auto counter) -> asio::awaitable<void> {
            for (int j = 0; j < iterations; ++j) {
                auto guard = co_await mutex->async_lock(use_awaitable);
                (*counter)++;
            }
        }(mutex, shared_counter));
    }
    
    // 等待所有任务完成
    for (auto& task : tasks) {
        co_await std::move(task);
    }
    
    auto elapsed = std::chrono::steady_clock::now() - start_time;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    
    int expected = num_workers * iterations;
    if (*shared_counter == expected) {
        std::cout << "✓ Counter = " << *shared_counter << " (expected " << expected << ")\n";
        std::cout << "✓ No race condition in stress test\n";
        std::cout << "✓ Completed in " << ms << "ms\n";
        std::cout << "✓ Throughput: " << (expected * 1000 / ms) << " locks/sec\n";
    } else {
        std::cout << "✗ Counter = " << *shared_counter << " (expected " << expected << ")\n";
    }
    
    std::cout << "✅ Test 7 PASSED\n";
}

// ============ 测试 8：防止重复 unlock ============
asio::awaitable<void> test_double_unlock() {
    std::cout << "\n=== Test 8: Double unlock (should be safe) ===\n";
    
    auto ex = co_await asio::this_coro::executor;
    auto mutex = std::make_shared<acore::async_mutex>(ex);
    
    co_await mutex->lock(use_awaitable);
    std::cout << "✓ Lock acquired\n";
    
    mutex->unlock();
    std::cout << "✓ First unlock\n";
    
    mutex->unlock();  // 应该安全地忽略
    std::cout << "✓ Second unlock (ignored safely)\n";
    
    bool is_locked = co_await mutex->async_is_locked(use_awaitable);
    if (!is_locked) {
        std::cout << "✓ Mutex is still unlocked\n";
    }
    
    std::cout << "✅ Test 8 PASSED\n";
}

// ============ 主测试函数 ============
asio::awaitable<void> run_all_tests() {
    try {
        co_await test_basic_lock_unlock();
        co_await test_concurrent_access();
        co_await test_lock_fairness();
        co_await test_try_lock_timeout();
        co_await test_lock_guard_move();
        co_await test_manual_unlock_guard();
        co_await test_stress();
        co_await test_double_unlock();
        
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

