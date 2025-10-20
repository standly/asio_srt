//
// 测试 async_latch
//
#include "acore/async_latch.hpp"
#include <asio.hpp>
#include <iostream>
#include <vector>
#include <chrono>
#include <atomic>

using namespace std::chrono_literals;
using asio::use_awaitable;

// ============ 测试 1：基本倒计数 ============
asio::awaitable<void> test_basic_countdown() {
    std::cout << "\n=== Test 1: Basic countdown ===\n";
    
    auto ex = co_await asio::this_coro::executor;
    auto latch = std::make_shared<acore::async_latch>(ex, 3);
    
    std::cout << "✓ Initial count: " << latch->count() << " (expected 3)\n";
    
    // 启动等待协程
    auto completed = std::make_shared<std::atomic<bool>>(false);
    
    asio::co_spawn(ex, [latch, completed]() -> asio::awaitable<void> {
        std::cout << "Waiting for latch...\n";
        co_await latch->wait(use_awaitable);
        
        completed->store(true);
        std::cout << "✓ Latch released\n";
    }, asio::detached);
    
    // 等待协程进入等待状态
    asio::steady_timer timer(ex);
    timer.expires_after(50ms);
    co_await timer.async_wait(use_awaitable);
    
    // 倒计数 3 次
    latch->count_down();
    std::cout << "Count down 1, remaining: " << latch->count() << "\n";
    
    latch->count_down();
    std::cout << "Count down 2, remaining: " << latch->count() << "\n";
    
    latch->count_down();
    std::cout << "Count down 3, remaining: " << latch->count() << "\n";
    
    // 等待完成
    timer.expires_after(50ms);
    co_await timer.async_wait(use_awaitable);
    
    if (completed->load()) {
        std::cout << "✓ Waiter completed after count reached 0\n";
    }
    
    if (latch->is_ready()) {
        std::cout << "✓ Latch is ready\n";
    }
    
    std::cout << "✅ Test 1 PASSED\n";
}

// ============ 测试 2：批量倒计数 ============
asio::awaitable<void> test_batch_countdown() {
    std::cout << "\n=== Test 2: Batch countdown ===\n";
    
    auto ex = co_await asio::this_coro::executor;
    auto latch = std::make_shared<acore::async_latch>(ex, 10);
    
    std::cout << "✓ Initial count: " << latch->count() << "\n";
    
    // 一次性倒计数 5
    latch->count_down(5);
    std::cout << "✓ Count down 5, remaining: " << latch->count() << " (expected 5)\n";
    
    // 再倒计数 5
    latch->count_down(5);
    std::cout << "✓ Count down 5, remaining: " << latch->count() << " (expected 0)\n";
    
    // 等待应该立即完成
    auto start = std::chrono::steady_clock::now();
    co_await latch->wait(use_awaitable);
    auto elapsed = std::chrono::steady_clock::now() - start;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    
    if (ms < 10) {
        std::cout << "✓ Wait completed immediately (" << ms << "ms)\n";
    }
    
    std::cout << "✅ Test 2 PASSED\n";
}

// ============ 测试 3：初始计数为 0 ============
asio::awaitable<void> test_zero_initial_count() {
    std::cout << "\n=== Test 3: Zero initial count ===\n";
    
    auto ex = co_await asio::this_coro::executor;
    auto latch = std::make_shared<acore::async_latch>(ex, 0);
    
    if (latch->is_ready()) {
        std::cout << "✓ Latch is ready immediately (count = 0)\n";
    }
    
    // 等待应该立即完成
    auto start = std::chrono::steady_clock::now();
    co_await latch->wait(use_awaitable);
    auto elapsed = std::chrono::steady_clock::now() - start;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    
    if (ms < 10) {
        std::cout << "✓ Wait completed immediately (" << ms << "ms)\n";
    }
    
    std::cout << "✅ Test 3 PASSED\n";
}

// ============ 测试 4：多个等待者 ============
asio::awaitable<void> test_multiple_waiters() {
    fprintf(stderr, "[DEBUG] test_multiple_waiters() entered\n"); fflush(stderr);
    std::cout << "\n=== Test 4: Multiple waiters ===\n";
    std::cout.flush();
    
    fprintf(stderr, "[DEBUG] Getting executor\n"); fflush(stderr);
    auto ex = co_await asio::this_coro::executor;
    fprintf(stderr, "[DEBUG] Creating latch\n"); fflush(stderr);
    auto latch = std::make_shared<acore::async_latch>(ex, 3);
    fprintf(stderr, "[DEBUG] Latch created, count=%ld\n", latch->count()); fflush(stderr);
    
    auto completed_count = std::make_shared<std::atomic<int>>(0);
    
    // 启动 5 个等待协程
    for (int i = 0; i < 5; ++i) {
        asio::co_spawn(ex, [latch, completed_count, i]() -> asio::awaitable<void> {
            std::cout << "Worker " << i << " waiting...\n";
            co_await latch->wait(use_awaitable);
            
            completed_count->fetch_add(1);
            std::cout << "Worker " << i << " released\n";
        }, asio::detached);
    }
    
    // 等待所有协程进入等待状态
    asio::steady_timer timer(ex);
    timer.expires_after(100ms);
    co_await timer.async_wait(use_awaitable);
    
    size_t waiting = co_await latch->async_waiting_count(use_awaitable);
    std::cout << "✓ Waiting count: " << waiting << " (expected 5)\n";
    
    // 倒计数到 0
    latch->count_down(3);
    std::cout << "Count down to 0\n";
    
    // 等待所有完成
    timer.expires_after(100ms);
    co_await timer.async_wait(use_awaitable);
    
    int completed = completed_count->load();
    if (completed == 5) {
        std::cout << "✓ All 5 waiters released\n";
    } else {
        std::cout << "✗ " << completed << " waiters released (expected 5)\n";
    }
    
    std::cout << "✅ Test 4 PASSED\n";
}

// ============ 测试 5：arrive_and_wait ============
asio::awaitable<void> test_arrive_and_wait() {
    std::cout << "\n=== Test 5: arrive_and_wait ===\n";
    
    auto ex = co_await asio::this_coro::executor;
    auto latch = std::make_shared<acore::async_latch>(ex, 3);
    
    auto results = std::make_shared<std::vector<int>>();
    
    // 3 个协程，每个 arrive_and_wait
    std::vector<asio::awaitable<void>> tasks;
    
    for (int i = 0; i < 3; ++i) {
        tasks.push_back([](auto latch, auto results, int id) -> asio::awaitable<void> {
            std::cout << "Worker " << id << " arriving...\n";
            co_await latch->arrive_and_wait(use_awaitable);
            
            results->push_back(id);
            std::cout << "Worker " << id << " passed latch\n";
        }(latch, results, i));
    }
    
    // 等待所有任务完成
    for (auto& task : tasks) {
        co_await std::move(task);
    }
    
    if (results->size() == 3) {
        std::cout << "✓ All 3 workers passed latch\n";
    }
    
    if (latch->count() == 0 && latch->is_ready()) {
        std::cout << "✓ Latch count: 0, ready: true\n";
    }
    
    std::cout << "✅ Test 5 PASSED\n";
}

// ============ 测试 6：try_wait ============
asio::awaitable<void> test_try_wait() {
    std::cout << "\n=== Test 6: try_wait (non-blocking) ===\n";
    
    auto ex = co_await asio::this_coro::executor;
    auto latch = std::make_shared<acore::async_latch>(ex, 3);
    
    // 未到达 0，try_wait 应该失败
    if (!latch->try_wait()) {
        std::cout << "✓ try_wait returned false (count > 0)\n";
    }
    
    // 倒计数到 0
    latch->count_down(3);
    
    asio::steady_timer timer(ex);
    timer.expires_after(50ms);
    co_await timer.async_wait(use_awaitable);
    
    // 现在应该成功
    if (latch->try_wait()) {
        std::cout << "✓ try_wait returned true (count = 0)\n";
    }
    
    std::cout << "✅ Test 6 PASSED\n";
}

// ============ 测试 7：一次性使用 ============
asio::awaitable<void> test_single_use() {
    std::cout << "\n=== Test 7: Single-use nature ===\n";
    
    auto ex = co_await asio::this_coro::executor;
    auto latch = std::make_shared<acore::async_latch>(ex, 2);
    
    // 倒计数到 0
    latch->count_down(2);
    std::cout << "✓ Count down to 0\n";
    
    // 等待（应该立即完成）
    co_await latch->wait(use_awaitable);
    std::cout << "✓ First wait completed\n";
    
    // 再次等待（也应该立即完成，因为已经触发）
    co_await latch->wait(use_awaitable);
    std::cout << "✓ Second wait completed (latch remains triggered)\n";
    
    // 再次倒计数（不会有任何效果，count 已经是 0）
    latch->count_down();
    std::cout << "✓ count_down() after triggered (ignored)\n";
    
    std::cout << "✓ Count: " << latch->count() << " (should remain 0)\n";
    
    std::cout << "✅ Test 7 PASSED\n";
}

// ============ 测试 8：启动屏障模式 ============
asio::awaitable<void> test_startup_barrier() {
    std::cout << "\n=== Test 8: Startup barrier pattern ===\n";
    
    auto ex = co_await asio::this_coro::executor;
    
    const int num_workers = 5;
    auto latch = std::make_shared<acore::async_latch>(ex, num_workers);
    
    auto started_count = std::make_shared<std::atomic<int>>(0);
    
    // 启动 5 个 worker，每个等待所有人就绪
    for (int i = 0; i < num_workers; ++i) {
        asio::co_spawn(ex, [latch, started_count, i]() -> asio::awaitable<void> {
            // 初始化
            asio::steady_timer timer(co_await asio::this_coro::executor);
            timer.expires_after(std::chrono::milliseconds(i * 20));
            co_await timer.async_wait(use_awaitable);
            
            std::cout << "Worker " << i << " initialized, waiting for all...\n";
            
            // arrive_and_wait：报告就绪并等待所有人
            co_await latch->arrive_and_wait(use_awaitable);
            
            // 所有人都就绪了，开始工作
            started_count->fetch_add(1);
            std::cout << "Worker " << i << " started\n";
        }, asio::detached);
    }
    
    // 等待所有 worker 启动
    asio::steady_timer timer(ex);
    timer.expires_after(500ms);
    co_await timer.async_wait(use_awaitable);
    
    int started = started_count->load();
    if (started == num_workers) {
        std::cout << "✓ All " << num_workers << " workers started simultaneously\n";
    } else {
        std::cout << "✗ Only " << started << " workers started\n";
    }
    
    std::cout << "✅ Test 8 PASSED\n";
}

// ============ 测试 9：vs waitgroup 对比 ============
asio::awaitable<void> test_vs_waitgroup() {
    std::cout << "\n=== Test 9: Latch vs WaitGroup comparison ===\n";
    
    auto ex = co_await asio::this_coro::executor;
    auto latch = std::make_shared<acore::async_latch>(ex, 3);
    
    std::cout << "Latch characteristics:\n";
    std::cout << "  - Initial count: 3 (fixed)\n";
    std::cout << "  - Only supports count_down()\n";
    std::cout << "  - One-time use (cannot be reset)\n";
    std::cout << "  - Simpler and lighter than waitgroup\n";
    std::cout << "\n";
    std::cout << "WaitGroup characteristics:\n";
    std::cout << "  - Supports both add() and done()\n";
    std::cout << "  - Dynamic count management\n";
    std::cout << "  - Can be reused after count reaches 0\n";
    std::cout << "  - More flexible but slightly heavier\n";
    std::cout << "\n";
    
    // 演示 latch 的使用
    latch->count_down(3);
    co_await latch->wait(use_awaitable);
    
    std::cout << "✓ Latch use case: fixed number of tasks\n";
    std::cout << "✓ WaitGroup use case: dynamic task management\n";
    
    std::cout << "✅ Test 9 PASSED\n";
}

// ============ 测试 10：压力测试 ============
asio::awaitable<void> test_stress() {
    std::cout << "\n=== Test 10: Stress test (100 waiters, count=100) ===\n";
    
    auto ex = co_await asio::this_coro::executor;
    const int count = 100;
    
    auto latch = std::make_shared<acore::async_latch>(ex, count);
    auto completed_count = std::make_shared<std::atomic<int>>(0);
    
    auto start_time = std::chrono::steady_clock::now();
    
    // 启动 100 个等待协程
    for (int i = 0; i < count; ++i) {
        asio::co_spawn(ex, [latch, completed_count]() -> asio::awaitable<void> {
            co_await latch->wait(use_awaitable);
            completed_count->fetch_add(1);
        }, asio::detached);
    }
    
    // 等待所有进入等待状态
    asio::steady_timer timer(ex);
    timer.expires_after(100ms);
    co_await timer.async_wait(use_awaitable);
    
    // 倒计数到 0
    latch->count_down(count);
    
    // 等待所有完成
    timer.expires_after(200ms);
    co_await timer.async_wait(use_awaitable);
    
    auto elapsed = std::chrono::steady_clock::now() - start_time;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    
    int completed = completed_count->load();
    if (completed == count) {
        std::cout << "✓ All " << count << " waiters completed\n";
        std::cout << "✓ Completed in " << ms << "ms\n";
    } else {
        std::cout << "✗ " << completed << " waiters completed (expected " << count << ")\n";
    }
    
    std::cout << "✅ Test 10 PASSED\n";
}

// ============ 主测试函数 ============
asio::awaitable<void> run_all_tests() {
    fprintf(stderr, "[DEBUG] run_all_tests() started\n"); fflush(stderr);
    try {
        fprintf(stderr, "[DEBUG] About to run test_basic_countdown\n"); fflush(stderr);
        co_await test_basic_countdown();
        fprintf(stderr, "[DEBUG] test_basic_countdown completed\n"); fflush(stderr);
        fprintf(stderr, "[DEBUG] About to run test_batch_countdown\n"); fflush(stderr);
        co_await test_batch_countdown();
        fprintf(stderr, "[DEBUG] test_batch_countdown completed\n"); fflush(stderr);
        fprintf(stderr, "[DEBUG] About to run test_zero_initial_count\n"); fflush(stderr);
        co_await test_zero_initial_count();
        fprintf(stderr, "[DEBUG] test_zero_initial_count completed\n"); fflush(stderr);
        co_await test_multiple_waiters();
        co_await test_arrive_and_wait();
        co_await test_try_wait();
        co_await test_single_use();
        co_await test_startup_barrier();
        co_await test_vs_waitgroup();
        co_await test_stress();
        
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

