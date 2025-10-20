//
// async_periodic_timer 使用示例
//
#include "acore/async_periodic_timer.hpp"
#include <asio.hpp>
#include <iostream>

using namespace std::chrono_literals;
using asio::use_awaitable;

// 示例 1：周期性心跳
asio::awaitable<void> example_heartbeat() {
    std::cout << "\n=== 示例 1: 周期性心跳 ===\n";
    
    auto ex = co_await asio::this_coro::executor;
    auto timer = std::make_shared<acore::async_periodic_timer>(ex, 1s);
    
    // 发送 5 次心跳
    for (int i = 0; i < 5; ++i) {
        co_await timer->async_next(use_awaitable);
        std::cout << "💓 Heartbeat " << (i + 1) << " sent\n";
    }
    
    timer->stop();
    std::cout << "心跳停止\n";
}

// 示例 2：统计上报
asio::awaitable<void> example_stats_reporter() {
    std::cout << "\n=== 示例 2: 定期统计上报 ===\n";
    
    auto ex = co_await asio::this_coro::executor;
    auto timer = std::make_shared<acore::async_periodic_timer>(ex, 500ms);
    
    int operations = 0;
    
    // 启动工作协程
    asio::co_spawn(ex, [&operations]() -> asio::awaitable<void> {
        for (int i = 0; i < 20; ++i) {
            asio::steady_timer t(co_await asio::this_coro::executor);
            t.expires_after(100ms);
            co_await t.async_wait(use_awaitable);
            operations++;
        }
    }, asio::detached);
    
    // 定期上报统计
    for (int i = 0; i < 5; ++i) {
        co_await timer->async_next(use_awaitable);
        std::cout << "📊 Stats report: " << operations << " operations completed\n";
    }
    
    timer->stop();
}

// 示例 3：一次性延迟
asio::awaitable<void> example_one_shot() {
    std::cout << "\n=== 示例 3: 一次性延迟 ===\n";
    
    auto ex = co_await asio::this_coro::executor;
    
    std::cout << "开始延迟任务...\n";
    
    acore::async_timer timer(ex);
    co_await timer.async_wait_for(2s, use_awaitable);
    
    std::cout << "✓ 2 秒后执行\n";
}

// 主函数
asio::awaitable<void> run_examples() {
    co_await example_heartbeat();
    co_await example_stats_reporter();
    co_await example_one_shot();
    
    std::cout << "\n✅ 所有示例完成\n";
}

int main() {
    asio::io_context io_context;
    asio::co_spawn(io_context, run_examples(), asio::detached);
    io_context.run();
    return 0;
}

