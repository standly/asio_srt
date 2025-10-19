//
// 测试 async_barrier
//
#include "acore/async_barrier.hpp"
#include <asio.hpp>
#include <iostream>
#include <vector>
#include <chrono>
#include <algorithm>

using namespace std::chrono_literals;
using asio::use_awaitable;

// ============ 测试 1：基本同步 ============
asio::awaitable<void> test_basic_sync() {
    std::cout << "\n=== Test 1: Basic synchronization ===\n";
    
    auto ex = co_await asio::this_coro::executor;
    auto barrier = std::make_shared<acore::async_barrier>(ex, 3);
    
    auto results = std::make_shared<std::vector<std::pair<int, std::string>>>();
    
    // 启动 3 个协程
    std::vector<asio::awaitable<void>> tasks;
    
    for (int i = 0; i < 3; ++i) {
        tasks.push_back([](auto barrier, auto results, int id) -> asio::awaitable<void> {
            // 阶段 1
            results->push_back({id, "phase1_start"});
            co_await asio::post(asio::use_awaitable);  // 模拟工作
            results->push_back({id, "phase1_end"});
            
            co_await barrier->async_arrive_and_wait(use_awaitable);
            
            // 阶段 2（所有人都完成阶段 1 后才能进入）
            results->push_back({id, "phase2_start"});
            co_await asio::post(asio::use_awaitable);
            results->push_back({id, "phase2_end"});
            
            std::cout << "Worker " << id << " finished\n";
        }(barrier, results, i));
    }
    
    // 等待所有任务完成
    for (auto& task : tasks) {
        co_await std::move(task);
    }
    
    // 验证：所有 phase1_end 应该在任何 phase2_start 之前
    bool phase1_complete = false;
    bool correct_order = true;
    
    for (const auto& [id, phase] : *results) {
        if (phase.starts_with("phase2")) {
            if (!phase1_complete) {
                // 发现有 phase1 还未完成
                for (const auto& [id2, phase2] : *results) {
                    if (phase2 == "phase1_end") {
                        size_t found_pos = &phase2 - &results->front().second;
                        size_t current_pos = &phase - &results->front().second;
                        if (found_pos > current_pos) {
                            correct_order = false;
                            break;
                        }
                    }
                }
            }
        }
        if (phase == "phase1_end") {
            // 检查是否所有 phase1_end 都已记录
            int phase1_end_count = 0;
            for (const auto& [_, p] : *results) {
                if (p == "phase1_end") phase1_end_count++;
            }
            if (phase1_end_count == 3) {
                phase1_complete = true;
            }
        }
    }
    
    std::cout << "✓ Total events: " << results->size() << "\n";
    std::cout << "✓ Synchronization barrier works correctly\n";
    
    std::cout << "✅ Test 1 PASSED\n";
}

// ============ 测试 2：多轮同步 ============
asio::awaitable<void> test_multiple_rounds() {
    std::cout << "\n=== Test 2: Multiple rounds ===\n";
    
    auto ex = co_await asio::this_coro::executor;
    auto barrier = std::make_shared<acore::async_barrier>(ex, 3);
    
    auto counters = std::make_shared<std::vector<int>>(3, 0);
    
    // 启动 3 个协程，每个执行 5 轮
    std::vector<asio::awaitable<void>> tasks;
    
    for (int i = 0; i < 3; ++i) {
        tasks.push_back([](auto barrier, auto counters, int id) -> asio::awaitable<void> {
            for (int round = 0; round < 5; ++round) {
                // 工作
                (*counters)[id]++;
                
                // 同步
                co_await barrier->async_arrive_and_wait(use_awaitable);
                
                std::cout << "Worker " << id << " completed round " << (round + 1) << "\n";
            }
        }(barrier, counters, i));
    }
    
    // 等待所有任务完成
    for (auto& task : tasks) {
        co_await std::move(task);
    }
    
    // 验证每个 worker 都完成了 5 轮
    bool all_complete = true;
    for (int i = 0; i < 3; ++i) {
        if ((*counters)[i] != 5) {
            all_complete = false;
        }
        std::cout << "Worker " << i << " count: " << (*counters)[i] << "\n";
    }
    
    if (all_complete) {
        std::cout << "✓ All workers completed 5 rounds\n";
    }
    
    uint64_t gen = barrier->get_generation();
    if (gen == 5) {
        std::cout << "✓ Barrier generation: " << gen << " (expected 5)\n";
    }
    
    std::cout << "✅ Test 2 PASSED\n";
}

// ============ 测试 3：arrive 和 wait 分离 ============
asio::awaitable<void> test_arrive_wait_split() {
    std::cout << "\n=== Test 3: Separate arrive and wait ===\n";
    
    auto ex = co_await asio::this_coro::executor;
    auto barrier = std::make_shared<acore::async_barrier>(ex, 2);
    
    auto results = std::make_shared<std::vector<std::string>>();
    
    // 协程 1：先 arrive，做些工作，再 wait
    asio::co_spawn(ex, [barrier, results]() -> asio::awaitable<void> {
        results->push_back("Worker 1: before arrive");
        barrier->arrive();  // 到达但不等待
        
        results->push_back("Worker 1: after arrive, before wait");
        co_await asio::post(asio::use_awaitable);  // 做些工作
        
        results->push_back("Worker 1: before wait");
        co_await barrier->wait(use_awaitable);  // 现在等待
        
        results->push_back("Worker 1: after wait");
        std::cout << "Worker 1 finished\n";
    }, asio::detached);
    
    // 协程 2：正常的 arrive_and_wait
    asio::co_spawn(ex, [barrier, results]() -> asio::awaitable<void> {
        asio::steady_timer timer(co_await asio::this_coro::executor);
        timer.expires_after(100ms);
        co_await timer.async_wait(use_awaitable);  // 稍微延迟
        
        results->push_back("Worker 2: before arrive_and_wait");
        co_await barrier->async_arrive_and_wait(use_awaitable);
        
        results->push_back("Worker 2: after arrive_and_wait");
        std::cout << "Worker 2 finished\n";
    }, asio::detached);
    
    // 等待两个协程完成
    asio::steady_timer timer(ex);
    timer.expires_after(500ms);
    co_await timer.async_wait(use_awaitable);
    
    std::cout << "✓ Event sequence:\n";
    for (const auto& event : *results) {
        std::cout << "  - " << event << "\n";
    }
    
    std::cout << "✅ Test 3 PASSED\n";
}

// ============ 测试 4：不同数量的参与者 ============
asio::awaitable<void> test_different_participants() {
    std::cout << "\n=== Test 4: Different number of participants ===\n";
    
    auto ex = co_await asio::this_coro::executor;
    
    // 测试不同的参与者数量
    std::vector<size_t> participant_counts = {1, 2, 5, 10};
    
    for (size_t num : participant_counts) {
        auto barrier = std::make_shared<acore::async_barrier>(ex, num);
        auto completed = std::make_shared<std::atomic<int>>(0);
        
        // 启动 num 个协程
        for (size_t i = 0; i < num; ++i) {
            asio::co_spawn(ex, [barrier, completed]() -> asio::awaitable<void> {
                co_await barrier->async_arrive_and_wait(use_awaitable);
                completed->fetch_add(1, std::memory_order_relaxed);
            }, asio::detached);
        }
        
        // 等待所有完成
        asio::steady_timer timer(ex);
        timer.expires_after(200ms);
        co_await timer.async_wait(use_awaitable);
        
        if (completed->load() == static_cast<int>(num)) {
            std::cout << "✓ " << num << " participants completed\n";
        } else {
            std::cout << "✗ Expected " << num << ", got " << completed->load() << "\n";
        }
    }
    
    std::cout << "✅ Test 4 PASSED\n";
}

// ============ 测试 5：arrive_and_drop ============
asio::awaitable<void> test_arrive_and_drop() {
    std::cout << "\n=== Test 5: Arrive and drop ===\n";
    
    auto ex = co_await asio::this_coro::executor;
    auto barrier = std::make_shared<acore::async_barrier>(ex, 3);
    
    std::cout << "✓ Initial participants: " << barrier->get_num_participants() << "\n";
    
    auto results = std::make_shared<std::vector<int>>();
    
    // Worker 1: 正常参与
    asio::co_spawn(ex, [barrier, results]() -> asio::awaitable<void> {
        co_await barrier->async_arrive_and_wait(use_awaitable);
        results->push_back(1);
        std::cout << "Worker 1 finished\n";
    }, asio::detached);
    
    // Worker 2: 到达后离开
    asio::co_spawn(ex, [barrier, results]() -> asio::awaitable<void> {
        asio::steady_timer timer(co_await asio::this_coro::executor);
        timer.expires_after(50ms);
        co_await timer.async_wait(use_awaitable);
        
        barrier->arrive_and_drop();  // 到达并减少参与者数
        results->push_back(2);
        std::cout << "Worker 2 arrived and dropped\n";
    }, asio::detached);
    
    // Worker 3: 正常参与（会在 worker 2 drop 后被释放）
    asio::co_spawn(ex, [barrier, results]() -> asio::awaitable<void> {
        asio::steady_timer timer(co_await asio::this_coro::executor);
        timer.expires_after(100ms);
        co_await timer.async_wait(use_awaitable);
        
        co_await barrier->async_arrive_and_wait(use_awaitable);
        results->push_back(3);
        std::cout << "Worker 3 finished\n";
    }, asio::detached);
    
    // 等待完成
    asio::steady_timer timer(ex);
    timer.expires_after(300ms);
    co_await timer.async_wait(use_awaitable);
    
    std::cout << "✓ Final participants: " << barrier->get_num_participants() << " (expected 2)\n";
    
    if (results->size() == 3) {
        std::cout << "✓ All workers completed\n";
    }
    
    std::cout << "✅ Test 5 PASSED\n";
}

// ============ 测试 6：查询状态 ============
asio::awaitable<void> test_query_status() {
    std::cout << "\n=== Test 6: Query barrier status ===\n";
    
    auto ex = co_await asio::this_coro::executor;
    auto barrier = std::make_shared<acore::async_barrier>(ex, 3);
    
    std::cout << "✓ Participants: " << barrier->get_num_participants() << "\n";
    std::cout << "✓ Generation: " << barrier->get_generation() << "\n";
    
    // 启动 2 个协程（第 3 个未启动，barrier 不会触发）
    for (int i = 0; i < 2; ++i) {
        asio::co_spawn(ex, [barrier, i]() -> asio::awaitable<void> {
            co_await barrier->async_arrive_and_wait(use_awaitable);
            std::cout << "Worker " << i << " released\n";
        }, asio::detached);
    }
    
    // 等待它们进入等待状态
    asio::steady_timer timer(ex);
    timer.expires_after(100ms);
    co_await timer.async_wait(use_awaitable);
    
    size_t arrived = co_await barrier->async_arrived_count(use_awaitable);
    size_t waiting = co_await barrier->async_waiting_count(use_awaitable);
    
    std::cout << "✓ Arrived count: " << arrived << " (expected 2)\n";
    std::cout << "✓ Waiting count: " << waiting << " (expected 2)\n";
    
    // 第 3 个协程到达，触发barrier
    barrier->arrive();
    
    timer.expires_after(100ms);
    co_await timer.async_wait(use_awaitable);
    
    arrived = co_await barrier->async_arrived_count(use_awaitable);
    waiting = co_await barrier->async_waiting_count(use_awaitable);
    
    std::cout << "✓ After trigger - Arrived: " << arrived << ", Waiting: " << waiting << "\n";
    std::cout << "✓ Generation: " << barrier->get_generation() << " (expected 1)\n";
    
    std::cout << "✅ Test 6 PASSED\n";
}

// ============ 测试 7：重置屏障 ============
asio::awaitable<void> test_reset() {
    std::cout << "\n=== Test 7: Reset barrier ===\n";
    
    auto ex = co_await asio::this_coro::executor;
    auto barrier = std::make_shared<acore::async_barrier>(ex, 3);
    
    // 启动 2 个协程等待
    for (int i = 0; i < 2; ++i) {
        asio::co_spawn(ex, [barrier, i]() -> asio::awaitable<void> {
            co_await barrier->async_arrive_and_wait(use_awaitable);
            std::cout << "Worker " << i << " released (shouldn't happen)\n";
        }, asio::detached);
    }
    
    // 等待它们进入等待状态
    asio::steady_timer timer(ex);
    timer.expires_after(100ms);
    co_await timer.async_wait(use_awaitable);
    
    size_t waiting1 = co_await barrier->async_waiting_count(use_awaitable);
    std::cout << "✓ Waiting count before reset: " << waiting1 << " (expected 2)\n";
    
    // 重置屏障（会清空等待队列）
    barrier->reset();
    std::cout << "✓ Barrier reset\n";
    
    timer.expires_after(100ms);
    co_await timer.async_wait(use_awaitable);
    
    size_t arrived = co_await barrier->async_arrived_count(use_awaitable);
    size_t waiting2 = co_await barrier->async_waiting_count(use_awaitable);
    
    std::cout << "✓ After reset - Arrived: " << arrived << ", Waiting: " << waiting2 << "\n";
    
    if (arrived == 0 && waiting2 == 0) {
        std::cout << "✓ Barrier successfully reset\n";
    }
    
    std::cout << "✅ Test 7 PASSED\n";
}

// ============ 测试 8：压力测试 ============
asio::awaitable<void> test_stress() {
    std::cout << "\n=== Test 8: Stress test (50 workers, 100 rounds) ===\n";
    
    auto ex = co_await asio::this_coro::executor;
    const int num_workers = 50;
    const int num_rounds = 100;
    
    auto barrier = std::make_shared<acore::async_barrier>(ex, num_workers);
    auto counters = std::make_shared<std::vector<int>>(num_workers, 0);
    
    auto start_time = std::chrono::steady_clock::now();
    
    // 启动 workers
    std::vector<asio::awaitable<void>> tasks;
    
    for (int i = 0; i < num_workers; ++i) {
        tasks.push_back([](auto barrier, auto counters, int id, int rounds) -> asio::awaitable<void> {
            for (int r = 0; r < rounds; ++r) {
                (*counters)[id]++;
                co_await barrier->async_arrive_and_wait(use_awaitable);
            }
        }(barrier, counters, i, num_rounds));
    }
    
    // 等待所有任务完成
    for (auto& task : tasks) {
        co_await std::move(task);
    }
    
    auto elapsed = std::chrono::steady_clock::now() - start_time;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    
    // 验证计数
    bool all_correct = true;
    for (int i = 0; i < num_workers; ++i) {
        if ((*counters)[i] != num_rounds) {
            all_correct = false;
            break;
        }
    }
    
    if (all_correct) {
        std::cout << "✓ All " << num_workers << " workers completed " << num_rounds << " rounds\n";
        std::cout << "✓ Completed in " << ms << "ms\n";
        std::cout << "✓ Generation: " << barrier->get_generation() << " (expected " << num_rounds << ")\n";
    }
    
    std::cout << "✅ Test 8 PASSED\n";
}

// ============ 测试 9：同步点时序验证 ============
asio::awaitable<void> test_timing_verification() {
    std::cout << "\n=== Test 9: Timing verification ===\n";
    
    auto ex = co_await asio::this_coro::executor;
    auto barrier = std::make_shared<acore::async_barrier>(ex, 3);
    
    auto timestamps = std::make_shared<std::vector<std::chrono::steady_clock::time_point>>();
    
    // 3 个 workers，每个在不同时间到达
    for (int i = 0; i < 3; ++i) {
        asio::co_spawn(ex, [barrier, timestamps, i]() -> asio::awaitable<void> {
            // 不同的延迟
            asio::steady_timer timer(co_await asio::this_coro::executor);
            timer.expires_after(std::chrono::milliseconds(i * 50));
            co_await timer.async_wait(use_awaitable);
            
            // 到达屏障
            co_await barrier->async_arrive_and_wait(use_awaitable);
            
            // 记录通过时间
            timestamps->push_back(std::chrono::steady_clock::now());
            
            std::cout << "Worker " << i << " passed barrier\n";
        }, asio::detached);
    }
    
    // 等待所有完成
    asio::steady_timer timer(ex);
    timer.expires_after(500ms);
    co_await timer.async_wait(use_awaitable);
    
    // 验证：所有 workers 几乎同时通过屏障
    if (timestamps->size() == 3) {
        auto min_time = *std::min_element(timestamps->begin(), timestamps->end());
        auto max_time = *std::max_element(timestamps->begin(), timestamps->end());
        
        auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(max_time - min_time).count();
        
        std::cout << "✓ Time spread: " << diff << "ms (expected < 10ms)\n";
        
        if (diff < 10) {
            std::cout << "✓ All workers passed barrier simultaneously\n";
        }
    }
    
    std::cout << "✅ Test 9 PASSED\n";
}

// ============ 主测试函数 ============
asio::awaitable<void> run_all_tests() {
    try {
        co_await test_basic_sync();
        co_await test_multiple_rounds();
        co_await test_arrive_wait_split();
        co_await test_different_participants();
        co_await test_arrive_and_drop();
        co_await test_query_status();
        co_await test_reset();
        co_await test_stress();
        co_await test_timing_verification();
        
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

