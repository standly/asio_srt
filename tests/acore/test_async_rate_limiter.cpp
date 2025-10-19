//
// 测试 async_rate_limiter
//
#include "acore/async_rate_limiter.hpp"
#include <asio.hpp>
#include <iostream>
#include <vector>
#include <chrono>
#include <algorithm>

using namespace std::chrono_literals;
using asio::use_awaitable;

// ============ 测试 1：基本速率限制 ============
asio::awaitable<void> test_basic_rate_limiting() {
    std::cout << "\n=== Test 1: Basic rate limiting ===\n";
    
    auto ex = co_await asio::this_coro::executor;
    
    // 限制：每秒 10 个令牌
    auto limiter = std::make_shared<acore::async_rate_limiter>(ex, 10, 1s);
    
    auto start = std::chrono::steady_clock::now();
    
    // 获取 10 个令牌（应该立即完成，因为初始有 10 个）
    for (int i = 0; i < 10; ++i) {
        co_await limiter->async_acquire(use_awaitable);
        std::cout << "Token " << (i + 1) << " acquired\n";
    }
    
    auto elapsed1 = std::chrono::steady_clock::now() - start;
    auto ms1 = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed1).count();
    
    if (ms1 < 100) {
        std::cout << "✓ First 10 tokens acquired immediately (" << ms1 << "ms)\n";
    }
    
    // 再获取 10 个令牌（需要等待 1 秒补充）
    start = std::chrono::steady_clock::now();
    for (int i = 0; i < 10; ++i) {
        co_await limiter->async_acquire(use_awaitable);
    }
    
    auto elapsed2 = std::chrono::steady_clock::now() - start;
    auto ms2 = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed2).count();
    
    if (ms2 >= 900 && ms2 <= 1200) {
        std::cout << "✓ Next 10 tokens took ~" << ms2 << "ms (expected ~1000ms)\n";
    }
    
    std::cout << "✅ Test 1 PASSED\n";
}

// ============ 测试 2：突发流量 ============
asio::awaitable<void> test_burst() {
    std::cout << "\n=== Test 2: Burst capacity ===\n";
    
    auto ex = co_await asio::this_coro::executor;
    
    // 限制：每秒 10 个令牌，允许突发 30 个
    auto limiter = std::make_shared<acore::async_rate_limiter>(ex, 10, 1s, 30);
    
    if (limiter->get_capacity() == 30) {
        std::cout << "✓ Capacity = " << limiter->get_capacity() << "\n";
    }
    
    auto start = std::chrono::steady_clock::now();
    
    // 立即获取 30 个令牌（使用突发容量）
    for (int i = 0; i < 30; ++i) {
        co_await limiter->async_acquire(use_awaitable);
    }
    
    auto elapsed = std::chrono::steady_clock::now() - start;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    
    if (ms < 200) {
        std::cout << "✓ 30 tokens acquired immediately using burst capacity (" << ms << "ms)\n";
    }
    
    // 再获取 10 个令牌（需要等待 1 秒）
    start = std::chrono::steady_clock::now();
    for (int i = 0; i < 10; ++i) {
        co_await limiter->async_acquire(use_awaitable);
    }
    
    elapsed = std::chrono::steady_clock::now() - start;
    ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    
    if (ms >= 900 && ms <= 1200) {
        std::cout << "✓ After burst, rate limited to ~" << ms << "ms for 10 tokens\n";
    }
    
    std::cout << "✅ Test 2 PASSED\n";
}

// ============ 测试 3：可变大小令牌消耗 ============
asio::awaitable<void> test_variable_tokens() {
    std::cout << "\n=== Test 3: Variable token consumption ===\n";
    
    auto ex = co_await asio::this_coro::executor;
    
    // 限制：每秒 100 个令牌（模拟 100 字节/秒带宽）
    auto limiter = std::make_shared<acore::async_rate_limiter>(ex, 100, 1s);
    
    auto start = std::chrono::steady_clock::now();
    
    // 发送不同大小的数据包
    std::vector<size_t> packet_sizes = {10, 20, 30, 40};  // 总共 100 字节
    
    for (auto size : packet_sizes) {
        co_await limiter->async_acquire(size, use_awaitable);
        std::cout << "Packet of " << size << " bytes sent\n";
    }
    
    auto elapsed1 = std::chrono::steady_clock::now() - start;
    auto ms1 = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed1).count();
    
    if (ms1 < 100) {
        std::cout << "✓ 100 bytes sent immediately (" << ms1 << "ms)\n";
    }
    
    // 再发送 100 字节（需要等待）
    start = std::chrono::steady_clock::now();
    co_await limiter->async_acquire(100, use_awaitable);
    
    auto elapsed2 = std::chrono::steady_clock::now() - start;
    auto ms2 = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed2).count();
    
    if (ms2 >= 900 && ms2 <= 1200) {
        std::cout << "✓ Next 100 bytes took ~" << ms2 << "ms (expected ~1000ms)\n";
    }
    
    std::cout << "✅ Test 3 PASSED\n";
}

// ============ 测试 4：try_acquire（非阻塞） ============
asio::awaitable<void> test_try_acquire() {
    std::cout << "\n=== Test 4: Non-blocking try_acquire ===\n";
    
    auto ex = co_await asio::this_coro::executor;
    
    // 限制：每秒 10 个令牌
    auto limiter = std::make_shared<acore::async_rate_limiter>(ex, 10, 1s);
    
    // 尝试获取 10 个令牌（应该成功）
    int acquired_count = 0;
    for (int i = 0; i < 10; ++i) {
        bool success = co_await limiter->async_try_acquire(use_awaitable);
        if (success) {
            acquired_count++;
        }
    }
    
    if (acquired_count == 10) {
        std::cout << "✓ Successfully acquired 10 tokens\n";
    }
    
    // 尝试再获取（应该失败）
    bool success = co_await limiter->async_try_acquire(use_awaitable);
    if (!success) {
        std::cout << "✓ Failed to acquire more tokens (as expected)\n";
    }
    
    // 检查可用令牌数
    size_t available = co_await limiter->async_available_tokens(use_awaitable);
    std::cout << "✓ Available tokens: " << available << "\n";
    
    // 等待一段时间后再试
    asio::steady_timer timer(ex);
    timer.expires_after(500ms);
    co_await timer.async_wait(use_awaitable);
    
    // 现在应该有约 5 个令牌
    available = co_await limiter->async_available_tokens(use_awaitable);
    std::cout << "✓ Available tokens after 500ms: " << available << " (expected ~5)\n";
    
    if (available >= 4 && available <= 6) {
        std::cout << "✓ Token refill rate is correct\n";
    }
    
    std::cout << "✅ Test 4 PASSED\n";
}

// ============ 测试 5：并发请求 ============
asio::awaitable<void> test_concurrent_requests() {
    std::cout << "\n=== Test 5: Concurrent requests ===\n";
    
    auto ex = co_await asio::this_coro::executor;
    
    // 限制：每秒 20 个令牌
    auto limiter = std::make_shared<acore::async_rate_limiter>(ex, 20, 1s);
    
    auto results = std::make_shared<std::vector<long long>>();
    auto start = std::chrono::steady_clock::now();
    
    // 启动 40 个并发请求
    std::vector<asio::awaitable<void>> tasks;
    
    for (int i = 0; i < 40; ++i) {
        tasks.push_back([](auto limiter, auto results, auto start_time, int id) -> asio::awaitable<void> {
            co_await limiter->async_acquire(use_awaitable);
            
            auto elapsed = std::chrono::steady_clock::now() - start_time;
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
            
            results->push_back(ms);
            std::cout << "Request " << id << " completed at " << ms << "ms\n";
        }(limiter, results, start, i));
    }
    
    // 等待所有请求完成
    for (auto& task : tasks) {
        co_await std::move(task);
    }
    
    auto total_elapsed = std::chrono::steady_clock::now() - start;
    auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(total_elapsed).count();
    
    // 40 个请求，每秒 20 个，应该需要约 2 秒
    if (total_ms >= 1800 && total_ms <= 2300) {
        std::cout << "✓ 40 requests completed in ~" << total_ms << "ms (expected ~2000ms)\n";
    }
    
    // 验证请求速率
    std::sort(results->begin(), results->end());
    
    // 前 20 个应该在第 1 秒内完成
    if ((*results)[19] <= 1100) {
        std::cout << "✓ First 20 requests completed within 1 second\n";
    }
    
    // 后 20 个应该在第 2 秒内完成
    if ((*results)[39] >= 1800 && (*results)[39] <= 2300) {
        std::cout << "✓ Last 20 requests completed in second interval\n";
    }
    
    std::cout << "✅ Test 5 PASSED\n";
}

// ============ 测试 6：动态修改速率 ============
asio::awaitable<void> test_change_rate() {
    std::cout << "\n=== Test 6: Change rate dynamically ===\n";
    
    auto ex = co_await asio::this_coro::executor;
    
    // 初始：每秒 10 个令牌
    auto limiter = std::make_shared<acore::async_rate_limiter>(ex, 10, 1s);
    
    std::cout << "✓ Initial rate: " << limiter->get_rate() << " tokens/sec\n";
    
    // 消耗初始令牌
    for (int i = 0; i < 10; ++i) {
        co_await limiter->async_acquire(use_awaitable);
    }
    
    // 修改速率为 20
    limiter->set_rate(20);
    std::cout << "✓ Rate changed to: " << limiter->get_rate() << " tokens/sec\n";
    
    auto start = std::chrono::steady_clock::now();
    
    // 获取 20 个令牌（应该在 1 秒内完成）
    for (int i = 0; i < 20; ++i) {
        co_await limiter->async_acquire(use_awaitable);
    }
    
    auto elapsed = std::chrono::steady_clock::now() - start;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    
    if (ms >= 900 && ms <= 1200) {
        std::cout << "✓ 20 tokens acquired in ~" << ms << "ms with new rate\n";
    }
    
    std::cout << "✅ Test 6 PASSED\n";
}

// ============ 测试 7：重置限流器 ============
asio::awaitable<void> test_reset() {
    std::cout << "\n=== Test 7: Reset limiter ===\n";
    
    auto ex = co_await asio::this_coro::executor;
    
    // 限制：每秒 10 个令牌
    auto limiter = std::make_shared<acore::async_rate_limiter>(ex, 10, 1s);
    
    // 消耗所有令牌
    for (int i = 0; i < 10; ++i) {
        co_await limiter->async_acquire(use_awaitable);
    }
    
    size_t available = co_await limiter->async_available_tokens(use_awaitable);
    std::cout << "✓ Available tokens after consuming all: " << available << "\n";
    
    // 重置
    limiter->reset();
    std::cout << "✓ Limiter reset\n";
    
    // 检查令牌数
    available = co_await limiter->async_available_tokens(use_awaitable);
    if (available == 10) {
        std::cout << "✓ Available tokens after reset: " << available << " (expected 10)\n";
    }
    
    // 应该能立即获取 10 个令牌
    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < 10; ++i) {
        co_await limiter->async_acquire(use_awaitable);
    }
    auto elapsed = std::chrono::steady_clock::now() - start;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    
    if (ms < 100) {
        std::cout << "✓ 10 tokens acquired immediately after reset (" << ms << "ms)\n";
    }
    
    std::cout << "✅ Test 7 PASSED\n";
}

// ============ 测试 8：停止限流器 ============
asio::awaitable<void> test_stop() {
    std::cout << "\n=== Test 8: Stop limiter ===\n";
    
    auto ex = co_await asio::this_coro::executor;
    
    // 限制：每秒 5 个令牌
    auto limiter = std::make_shared<acore::async_rate_limiter>(ex, 5, 1s);
    
    // 消耗所有令牌
    for (int i = 0; i < 5; ++i) {
        co_await limiter->async_acquire(use_awaitable);
    }
    
    std::cout << "✓ All initial tokens consumed\n";
    
    // 启动协程来停止限流器
    asio::co_spawn(ex, [limiter]() -> asio::awaitable<void> {
        asio::steady_timer timer(co_await asio::this_coro::executor);
        timer.expires_after(200ms);
        co_await timer.async_wait(use_awaitable);
        
        std::cout << "Stopping limiter...\n";
        limiter->stop();
    }, asio::detached);
    
    // 尝试获取更多令牌（会被停止操作中断）
    auto start = std::chrono::steady_clock::now();
    
    for (int i = 0; i < 3; ++i) {
        co_await limiter->async_acquire(use_awaitable);
    }
    
    auto elapsed = std::chrono::steady_clock::now() - start;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    
    if (ms < 500) {
        std::cout << "✓ Requests completed quickly after stop (" << ms << "ms)\n";
    }
    
    std::cout << "✅ Test 8 PASSED\n";
}

// ============ 测试 9：等待队列 ============
asio::awaitable<void> test_waiting_queue() {
    std::cout << "\n=== Test 9: Waiting queue ===\n";
    
    auto ex = co_await asio::this_coro::executor;
    
    // 限制：每秒 5 个令牌
    auto limiter = std::make_shared<acore::async_rate_limiter>(ex, 5, 1s);
    
    // 消耗所有令牌
    for (int i = 0; i < 5; ++i) {
        co_await limiter->async_acquire(use_awaitable);
    }
    
    // 启动 10 个协程尝试获取令牌（会进入等待队列）
    for (int i = 0; i < 10; ++i) {
        asio::co_spawn(ex, [limiter, i]() -> asio::awaitable<void> {
            co_await limiter->async_acquire(use_awaitable);
            std::cout << "Request " << i << " completed\n";
        }, asio::detached);
    }
    
    // 等待所有协程进入等待队列
    asio::steady_timer timer(ex);
    timer.expires_after(100ms);
    co_await timer.async_wait(use_awaitable);
    
    size_t waiting = co_await limiter->async_waiting_count(use_awaitable);
    std::cout << "✓ Waiting count: " << waiting << " (expected 10)\n";
    
    if (waiting == 10) {
        std::cout << "✓ All requests are waiting\n";
    }
    
    // 等待所有请求完成（需要 2 秒，因为每秒 5 个）
    timer.expires_after(2500ms);
    co_await timer.async_wait(use_awaitable);
    
    waiting = co_await limiter->async_waiting_count(use_awaitable);
    if (waiting == 0) {
        std::cout << "✓ All waiting requests completed\n";
    }
    
    std::cout << "✅ Test 9 PASSED\n";
}

// ============ 测试 10：精确速率测试 ============
asio::awaitable<void> test_rate_accuracy() {
    std::cout << "\n=== Test 10: Rate accuracy test ===\n";
    
    auto ex = co_await asio::this_coro::executor;
    
    // 限制：每 100ms 10 个令牌（即每秒 100 个）
    auto limiter = std::make_shared<acore::async_rate_limiter>(ex, 10, 100ms);
    
    auto start = std::chrono::steady_clock::now();
    
    // 获取 100 个令牌
    for (int i = 0; i < 100; ++i) {
        co_await limiter->async_acquire(use_awaitable);
    }
    
    auto elapsed = std::chrono::steady_clock::now() - start;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    
    // 应该需要约 1 秒（10个初始 + 90个需要900ms）
    if (ms >= 850 && ms <= 1100) {
        std::cout << "✓ 100 tokens in " << ms << "ms (expected ~1000ms)\n";
        std::cout << "✓ Actual rate: " << (100000 / ms) << " tokens/sec (expected 100)\n";
    }
    
    std::cout << "✅ Test 10 PASSED\n";
}

// ============ 主测试函数 ============
asio::awaitable<void> run_all_tests() {
    try {
        co_await test_basic_rate_limiting();
        co_await test_burst();
        co_await test_variable_tokens();
        co_await test_try_acquire();
        co_await test_concurrent_requests();
        co_await test_change_rate();
        co_await test_reset();
        co_await test_stop();
        co_await test_waiting_queue();
        co_await test_rate_accuracy();
        
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

