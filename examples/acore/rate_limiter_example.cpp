//
// async_rate_limiter 使用示例
//
#include "acore/async_rate_limiter.hpp"
#include <asio.hpp>
#include <iostream>

using namespace std::chrono_literals;
using asio::use_awaitable;

// 示例 1：API 调用频率限制
asio::awaitable<void> example_api_rate_limit() {
    std::cout << "\n=== 示例 1: API 调用频率限制 ===\n";
    
    auto ex = co_await asio::this_coro::executor;
    
    // 限制：每秒最多 3 个请求
    auto limiter = std::make_shared<acore::async_rate_limiter>(ex, 3, 1s);
    
    auto start = std::chrono::steady_clock::now();
    
    // 尝试发送 6 个请求
    for (int i = 0; i < 6; ++i) {
        co_await limiter->async_acquire(use_awaitable);
        
        auto elapsed = std::chrono::steady_clock::now() - start;
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
        
        std::cout << "Request " << (i + 1) << " sent at " << ms << "ms\n";
    }
    
    std::cout << "✓ 前 3 个请求立即发送，后 3 个被限速\n";
}

// 示例 2：带宽限制
asio::awaitable<void> example_bandwidth_limit() {
    std::cout << "\n=== 示例 2: 带宽限制（按大小消耗令牌）===\n";
    
    auto ex = co_await asio::this_coro::executor;
    
    // 限制：每秒 1000 字节，允许突发 2000 字节
    auto limiter = std::make_shared<acore::async_rate_limiter>(ex, 1000, 1s, 2000);
    
    std::vector<size_t> packet_sizes = {500, 500, 500, 500};  // 总共 2000 字节
    
    auto start = std::chrono::steady_clock::now();
    
    for (size_t i = 0; i < packet_sizes.size(); ++i) {
        size_t size = packet_sizes[i];
        
        // 按包大小消耗令牌
        co_await limiter->async_acquire(size, use_awaitable);
        
        auto elapsed = std::chrono::steady_clock::now() - start;
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
        
        std::cout << "Packet " << (i + 1) << " (" << size << " bytes) sent at " << ms << "ms\n";
    }
}

// 示例 3：非阻塞检查
asio::awaitable<void> example_try_acquire() {
    std::cout << "\n=== 示例 3: 非阻塞令牌检查 ===\n";
    
    auto ex = co_await asio::this_coro::executor;
    auto limiter = std::make_shared<acore::async_rate_limiter>(ex, 5, 1s);
    
    // 快速尝试
    for (int i = 0; i < 10; ++i) {
        bool acquired = co_await limiter->async_try_acquire(use_awaitable);
        
        if (acquired) {
            std::cout << "✓ Request " << (i + 1) << " accepted\n";
        } else {
            std::cout << "✗ Request " << (i + 1) << " rejected (rate limit)\n";
        }
    }
}

// 主函数
asio::awaitable<void> run_examples() {
    co_await example_api_rate_limit();
    co_await example_bandwidth_limit();
    co_await example_try_acquire();
    
    std::cout << "\n✅ 所有示例完成\n";
}

int main() {
    asio::io_context io_context;
    asio::co_spawn(io_context, run_examples(), asio::detached);
    io_context.run();
    return 0;
}

