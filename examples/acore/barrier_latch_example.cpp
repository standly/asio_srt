//
// async_barrier 和 async_latch 使用示例
//
#include "acore/async_barrier.hpp"
#include "acore/async_latch.hpp"
#include <asio.hpp>
#include <iostream>

using namespace std::chrono_literals;
using asio::use_awaitable;

// 示例 1：使用 barrier 进行多阶段同步
asio::awaitable<void> example_multi_phase_processing() {
    std::cout << "\n=== 示例 1: Barrier - 多阶段处理 ===\n";
    
    auto ex = co_await asio::this_coro::executor;
    auto barrier = std::make_shared<acore::async_barrier>(ex, 3);
    
    // 3 个 worker，每个经历 3 个阶段
    for (int i = 0; i < 3; ++i) {
        asio::co_spawn(ex, [barrier, i]() -> asio::awaitable<void> {
            // 阶段 1：准备数据
            std::cout << "Worker " << i << ": 准备数据...\n";
            asio::steady_timer timer(co_await asio::this_coro::executor);
            timer.expires_after(std::chrono::milliseconds(100 * (i + 1)));
            co_await timer.async_wait(use_awaitable);
            
            co_await barrier->async_arrive_and_wait(use_awaitable);
            std::cout << "Worker " << i << ": ✓ 阶段 1 同步完成\n";
            
            // 阶段 2：处理数据
            std::cout << "Worker " << i << ": 处理数据...\n";
            timer.expires_after(50ms);
            co_await timer.async_wait(use_awaitable);
            
            co_await barrier->async_arrive_and_wait(use_awaitable);
            std::cout << "Worker " << i << ": ✓ 阶段 2 同步完成\n";
            
            // 阶段 3：输出结果
            std::cout << "Worker " << i << ": 输出结果\n";
            
            co_await barrier->async_arrive_and_wait(use_awaitable);
            std::cout << "Worker " << i << ": ✓ 所有阶段完成\n";
        }, asio::detached);
    }
    
    asio::steady_timer timer(ex);
    timer.expires_after(1s);
    co_await timer.async_wait(use_awaitable);
}

// 示例 2：使用 latch 作为启动屏障
asio::awaitable<void> example_startup_barrier() {
    std::cout << "\n=== 示例 2: Latch - 启动屏障 ===\n";
    
    auto ex = co_await asio::this_coro::executor;
    const int num_workers = 5;
    
    auto latch = std::make_shared<acore::async_latch>(ex, num_workers);
    
    std::cout << "启动 " << num_workers << " 个 worker...\n";
    
    // 启动 workers
    for (int i = 0; i < num_workers; ++i) {
        asio::co_spawn(ex, [latch, i]() -> asio::awaitable<void> {
            // 初始化（不同的时间）
            asio::steady_timer timer(co_await asio::this_coro::executor);
            timer.expires_after(std::chrono::milliseconds(50 * (i + 1)));
            co_await timer.async_wait(use_awaitable);
            
            std::cout << "Worker " << i << " initialized, waiting for others...\n";
            
            // 报告就绪并等待所有人
            co_await latch->arrive_and_wait(use_awaitable);
            
            // 所有人都就绪，开始工作
            std::cout << "Worker " << i << " started working!\n";
        }, asio::detached);
    }
    
    asio::steady_timer timer(ex);
    timer.expires_after(500ms);
    co_await timer.async_wait(use_awaitable);
    
    std::cout << "✓ 所有 workers 已同时启动\n";
}

// 示例 3：等待固定数量的任务完成
asio::awaitable<void> example_wait_tasks() {
    std::cout << "\n=== 示例 3: Latch - 等待任务完成 ===\n";
    
    auto ex = co_await asio::this_coro::executor;
    
    auto latch = std::make_shared<acore::async_latch>(ex, 3);
    
    std::cout << "启动 3 个异步任务...\n";
    
    // 启动 3 个任务
    for (int i = 0; i < 3; ++i) {
        asio::co_spawn(ex, [latch, i]() -> asio::awaitable<void> {
            // 模拟异步工作
            asio::steady_timer timer(co_await asio::this_coro::executor);
            timer.expires_after(std::chrono::milliseconds(200 * (i + 1)));
            co_await timer.async_wait(use_awaitable);
            
            std::cout << "Task " << (i + 1) << " completed\n";
            latch->count_down();
        }, asio::detached);
    }
    
    std::cout << "等待所有任务完成...\n";
    co_await latch->wait(use_awaitable);
    
    std::cout << "✓ 所有 3 个任务已完成！\n";
}

// 主函数
asio::awaitable<void> run_examples() {
    co_await example_multi_phase_processing();
    co_await example_startup_barrier();
    co_await example_wait_tasks();
    
    std::cout << "\n✅ 所有示例完成\n";
}

int main() {
    asio::io_context io_context;
    asio::co_spawn(io_context, run_examples(), asio::detached);
    io_context.run();
    return 0;
}

