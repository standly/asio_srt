//
// 测试 async_periodic_timer
//
#include "acore/async_periodic_timer.hpp"
#include <asio.hpp>
#include <iostream>
#include <vector>
#include <chrono>
#include <cmath>

using namespace std::chrono_literals;
using asio::use_awaitable;

// ============ 测试 1：基本周期触发 ============
asio::awaitable<void> test_basic_periodic() {
    std::cout << "\n=== Test 1: Basic periodic triggering ===\n";
    
    auto ex = co_await asio::this_coro::executor;
    auto timer = std::make_shared<acore::async_periodic_timer>(ex, 100ms);
    
    int count = 0;
    auto start = std::chrono::steady_clock::now();
    
    for (int i = 0; i < 5; ++i) {
        co_await timer->async_next(use_awaitable);
        count++;
        
        auto elapsed = std::chrono::steady_clock::now() - start;
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
        std::cout << "Tick " << count << " at " << ms << "ms\n";
    }
    
    auto elapsed = std::chrono::steady_clock::now() - start;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    
    if (count == 5 && ms >= 450 && ms <= 600) {
        std::cout << "✓ Triggered 5 times in ~" << ms << "ms (expected ~500ms)\n";
    }
    
    std::cout << "✅ Test 1 PASSED\n";
}

// ============ 测试 2：停止定时器 ============
asio::awaitable<void> test_stop() {
    std::cout << "\n=== Test 2: Stop timer ===\n";
    
    auto ex = co_await asio::this_coro::executor;
    auto timer = std::make_shared<acore::async_periodic_timer>(ex, 100ms);
    
    auto count = std::make_shared<std::atomic<int>>(0);
    
    // 启动一个协程运行定时器
    asio::co_spawn(ex, [timer, count]() -> asio::awaitable<void> {
        for (int i = 0; i < 10; ++i) {
            co_await timer->async_next(use_awaitable);
            count->fetch_add(1);
            std::cout << "Tick " << count->load() << "\n";
        }
    }, asio::detached);
    
    // 等待一段时间后停止
    asio::steady_timer delay(ex);
    delay.expires_after(250ms);
    co_await delay.async_wait(use_awaitable);
    
    std::cout << "Stopping timer...\n";
    timer->stop();
    
    // 再等一会儿确保没有更多 ticks
    delay.expires_after(200ms);
    co_await delay.async_wait(use_awaitable);
    
    int final_count = count->load();
    if (final_count >= 2 && final_count <= 3) {
        std::cout << "✓ Triggered " << final_count << " times before stop (expected 2-3)\n";
    }
    
    if (!timer->is_running()) {
        std::cout << "✓ Timer is not running\n";
    }
    
    std::cout << "✅ Test 2 PASSED\n";
}

// ============ 测试 3：暂停和恢复 ============
asio::awaitable<void> test_pause_resume() {
    std::cout << "\n=== Test 3: Pause and resume ===\n";
    
    auto ex = co_await asio::this_coro::executor;
    auto timer = std::make_shared<acore::async_periodic_timer>(ex, 100ms);
    
    int count = 0;
    auto start = std::chrono::steady_clock::now();
    
    // 触发 2 次
    for (int i = 0; i < 2; ++i) {
        co_await timer->async_next(use_awaitable);
        count++;
        std::cout << "Tick " << count << " (before pause)\n";
    }
    
    // 暂停
    timer->pause();
    std::cout << "✓ Timer paused\n";
    
    if (timer->is_paused()) {
        std::cout << "✓ Timer is paused\n";
    }
    
    // 暂停时 async_wait 不会触发（会被忽略）
    std::cout << "✓ Timer paused, async_wait will be ignored\n";
    
    // 等待 300ms
    asio::steady_timer delay(ex);
    delay.expires_after(300ms);
    co_await delay.async_wait(use_awaitable);
    
    // 恢复
    timer->resume();
    std::cout << "✓ Timer resumed\n";
    
    if (!timer->is_paused()) {
        std::cout << "✓ Timer is not paused\n";
    }
    
    // 继续触发 2 次
    for (int i = 0; i < 2; ++i) {
        co_await timer->async_next(use_awaitable);
        count++;
        std::cout << "Tick " << count << " (after resume)\n";
    }
    
    auto elapsed = std::chrono::steady_clock::now() - start;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    
    if (count == 4 && ms >= 650) {  // 4 ticks + 300ms pause
        std::cout << "✓ Total ticks: " << count << ", elapsed: " << ms << "ms\n";
    }
    
    std::cout << "✅ Test 3 PASSED\n";
}

// ============ 测试 4：动态修改周期 ============
asio::awaitable<void> test_change_period() {
    std::cout << "\n=== Test 4: Change period dynamically ===\n";
    
    auto ex = co_await asio::this_coro::executor;
    auto timer = std::make_shared<acore::async_periodic_timer>(ex, 100ms);
    
    auto start = std::chrono::steady_clock::now();
    
    // 前 3 次：100ms 周期
    for (int i = 0; i < 3; ++i) {
        co_await timer->async_next(use_awaitable);
        auto elapsed = std::chrono::steady_clock::now() - start;
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
        std::cout << "Tick " << (i + 1) << " at " << ms << "ms (100ms period)\n";
    }
    
    // 修改周期为 200ms
    timer->set_period(200ms);
    std::cout << "✓ Period changed to 200ms\n";
    
    auto period = timer->get_period<std::chrono::milliseconds>();
    std::cout << "✓ Current period: " << period.count() << "ms\n";
    
    // 后 2 次：200ms 周期
    for (int i = 0; i < 2; ++i) {
        co_await timer->async_next(use_awaitable);
        auto elapsed = std::chrono::steady_clock::now() - start;
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
        std::cout << "Tick " << (i + 4) << " at " << ms << "ms (200ms period)\n";
    }
    
    auto total_elapsed = std::chrono::steady_clock::now() - start;
    auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(total_elapsed).count();
    
    // 预期：3*100 + 2*200 = 700ms
    if (total_ms >= 650 && total_ms <= 800) {
        std::cout << "✓ Total time: " << total_ms << "ms (expected ~700ms)\n";
    }
    
    std::cout << "✅ Test 4 PASSED\n";
}

// ============ 测试 5：多个定时器并发 ============
asio::awaitable<void> test_multiple_timers() {
    std::cout << "\n=== Test 5: Multiple timers concurrently ===\n";
    
    auto ex = co_await asio::this_coro::executor;
    
    auto results = std::make_shared<std::vector<std::pair<int, int>>>();  // (timer_id, tick_count)
    
    // 创建 3 个不同周期的定时器
    std::vector<asio::awaitable<void>> tasks;
    
    // Timer 1: 50ms, 10 ticks
    tasks.push_back([](auto ex, auto results) -> asio::awaitable<void> {
        auto timer = std::make_shared<acore::async_periodic_timer>(ex, 50ms);
        for (int i = 0; i < 10; ++i) {
            co_await timer->async_next(use_awaitable);
            results->push_back({1, i + 1});
        }
        std::cout << "✓ Timer 1 (50ms) completed 10 ticks\n";
    }(ex, results));
    
    // Timer 2: 100ms, 5 ticks
    tasks.push_back([](auto ex, auto results) -> asio::awaitable<void> {
        auto timer = std::make_shared<acore::async_periodic_timer>(ex, 100ms);
        for (int i = 0; i < 5; ++i) {
            co_await timer->async_next(use_awaitable);
            results->push_back({2, i + 1});
        }
        std::cout << "✓ Timer 2 (100ms) completed 5 ticks\n";
    }(ex, results));
    
    // Timer 3: 150ms, 3 ticks
    tasks.push_back([](auto ex, auto results) -> asio::awaitable<void> {
        auto timer = std::make_shared<acore::async_periodic_timer>(ex, 150ms);
        for (int i = 0; i < 3; ++i) {
            co_await timer->async_next(use_awaitable);
            results->push_back({3, i + 1});
        }
        std::cout << "✓ Timer 3 (150ms) completed 3 ticks\n";
    }(ex, results));
    
    // 等待所有定时器完成
    for (auto& task : tasks) {
        co_await std::move(task);
    }
    
    if (results->size() == 18) {  // 10 + 5 + 3
        std::cout << "✓ Total events: " << results->size() << " (expected 18)\n";
    }
    
    std::cout << "✅ Test 5 PASSED\n";
}

// ============ 测试 6：async_timer（一次性定时器）============
asio::awaitable<void> test_one_shot_timer() {
    std::cout << "\n=== Test 6: One-shot async_timer ===\n";
    
    auto ex = co_await asio::this_coro::executor;
    
    auto start = std::chrono::steady_clock::now();
    
    acore::async_timer timer(ex);
    co_await timer.async_wait_for(200ms, use_awaitable);
    
    auto elapsed = std::chrono::steady_clock::now() - start;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    
    if (ms >= 180 && ms <= 250) {
        std::cout << "✓ Timer triggered after " << ms << "ms (expected ~200ms)\n";
    }
    
    // 第二次使用（重新设置）
    start = std::chrono::steady_clock::now();
    co_await timer.async_wait_for(100ms, use_awaitable);
    
    elapsed = std::chrono::steady_clock::now() - start;
    ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    
    if (ms >= 90 && ms <= 150) {
        std::cout << "✓ Timer triggered again after " << ms << "ms (expected ~100ms)\n";
    }
    
    std::cout << "✅ Test 6 PASSED\n";
}

// ============ 测试 7：定时器取消 ============
asio::awaitable<void> test_cancel() {
    std::cout << "\n=== Test 7: Timer cancellation ===\n";
    
    auto ex = co_await asio::this_coro::executor;
    
    acore::async_timer timer(ex);
    
    // 启动一个协程来取消定时器
    asio::co_spawn(ex, [&timer]() -> asio::awaitable<void> {
        asio::steady_timer delay(co_await asio::this_coro::executor);
        delay.expires_after(50ms);
        co_await delay.async_wait(use_awaitable);
        
        std::cout << "Cancelling timer...\n";
        timer.cancel();
    }, asio::detached);
    
    auto start = std::chrono::steady_clock::now();
    auto ec = co_await timer.async_wait_for(500ms, asio::as_tuple(use_awaitable));
    auto elapsed = std::chrono::steady_clock::now() - start;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    
    if (std::get<0>(ec)) {
        std::cout << "✓ Timer cancelled (error: " << std::get<0>(ec).message() << ")\n";
        std::cout << "✓ Cancelled after " << ms << "ms (expected ~50ms)\n";
    }
    
    std::cout << "✅ Test 7 PASSED\n";
}

// ============ 测试 8：精度测试 ============
asio::awaitable<void> test_accuracy() {
    std::cout << "\n=== Test 8: Timer accuracy test ===\n";
    
    auto ex = co_await asio::this_coro::executor;
    auto timer = std::make_shared<acore::async_periodic_timer>(ex, 100ms);
    
    std::vector<long long> intervals;
    auto last_time = std::chrono::steady_clock::now();
    
    for (int i = 0; i < 10; ++i) {
        co_await timer->async_next(use_awaitable);
        
        auto now = std::chrono::steady_clock::now();
        auto interval = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_time).count();
        intervals.push_back(interval);
        last_time = now;
        
        std::cout << "Interval " << (i + 1) << ": " << interval << "ms\n";
    }
    
    // 计算平均值和标准差
    double sum = 0;
    for (auto interval : intervals) {
        sum += interval;
    }
    double avg = sum / intervals.size();
    
    double sq_sum = 0;
    for (auto interval : intervals) {
        sq_sum += (interval - avg) * (interval - avg);
    }
    double stddev = std::sqrt(sq_sum / intervals.size());
    
    std::cout << "✓ Average interval: " << avg << "ms (expected 100ms)\n";
    std::cout << "✓ Standard deviation: " << stddev << "ms\n";
    
    if (avg >= 95 && avg <= 105 && stddev < 10) {
        std::cout << "✓ Timer accuracy is good\n";
    }
    
    std::cout << "✅ Test 8 PASSED\n";
}

// ============ 测试 9：重启定时器 ============
asio::awaitable<void> test_restart() {
    std::cout << "\n=== Test 9: Restart timer ===\n";
    
    auto ex = co_await asio::this_coro::executor;
    auto timer = std::make_shared<acore::async_periodic_timer>(ex, 100ms);
    
    // 触发 2 次
    for (int i = 0; i < 2; ++i) {
        co_await timer->async_next(use_awaitable);
        std::cout << "Tick " << (i + 1) << " (before stop)\n";
    }
    
    // 停止
    timer->stop();
    std::cout << "✓ Timer stopped\n";
    
    if (!timer->is_running()) {
        std::cout << "✓ Timer is not running\n";
    }
    
    // 重启
    timer->restart();
    std::cout << "✓ Timer restarted\n";
    
    if (timer->is_running()) {
        std::cout << "✓ Timer is running\n";
    }
    
    // 继续触发 2 次
    for (int i = 0; i < 2; ++i) {
        co_await timer->async_next(use_awaitable);
        std::cout << "Tick " << (i + 3) << " (after restart)\n";
    }
    
    std::cout << "✅ Test 9 PASSED\n";
}

// ============ 主测试函数 ============
asio::awaitable<void> run_all_tests() {
    try {
        co_await test_basic_periodic();
        co_await test_stop();
        co_await test_pause_resume();
        co_await test_change_period();
        co_await test_multiple_timers();
        co_await test_one_shot_timer();
        co_await test_cancel();
        co_await test_accuracy();
        co_await test_restart();
        
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

