//
// async_waitgroup 简单示例 - 等待多个下载任务完成
//
#include "async_waitgroup.hpp"
#include <iostream>
#include <chrono>
#include <string>
#include <vector>

using namespace std::chrono_literals;

// 模拟异步下载
asio::awaitable<void> download_file(const std::string& url, std::chrono::milliseconds delay) {
    auto ex = co_await asio::this_coro::executor;
    auto& io_context = static_cast<asio::io_context&>(ex.context());
    
    std::cout << "  开始下载: " << url << "\n";
    
    asio::steady_timer timer(io_context);
    timer.expires_after(delay);
    co_await timer.async_wait(asio::use_awaitable);
    
    std::cout << "  完成下载: " << url << "\n";
}

// 示例 1: 基本用法 - 等待所有下载完成
asio::awaitable<void> example_basic() {
    auto ex = co_await asio::this_coro::executor;
    auto wg = std::make_shared<acore::async_waitgroup>(ex);
    
    std::cout << "=== 示例 1: 基本用法 ===\n";
    
    std::vector<std::string> urls = {
        "http://example.com/file1.dat",
        "http://example.com/file2.dat",
        "http://example.com/file3.dat",
    };
    
    // 添加任务计数
    wg->add(urls.size());
    
    // 启动下载任务
    for (size_t i = 0; i < urls.size(); ++i) {
        asio::co_spawn(ex, [wg, url = urls[i], i]() -> asio::awaitable<void> {
            co_await download_file(url, std::chrono::milliseconds(100 * (i + 1)));
            wg->done();  // 完成一个任务
        }, asio::detached);
    }
    
    // 等待所有任务完成
    std::cout << "等待所有下载完成...\n";
    co_await wg->wait(asio::use_awaitable);
    
    std::cout << "✓ 所有文件下载完成！\n\n";
}

// 示例 2: 带超时的等待
asio::awaitable<void> example_timeout() {
    auto ex = co_await asio::this_coro::executor;
    auto& io_context = static_cast<asio::io_context&>(ex.context());
    auto wg = std::make_shared<acore::async_waitgroup>(ex);
    
    std::cout << "=== 示例 2: 超时等待 ===\n";
    
    // 启动一个慢任务
    wg->add(1);
    asio::co_spawn(ex, [wg, &io_context]() -> asio::awaitable<void> {
        asio::steady_timer timer(io_context);
        timer.expires_after(5s);
        co_await timer.async_wait(asio::use_awaitable);
        std::cout << "  慢任务完成\n";
        wg->done();
    }, asio::detached);
    
    // 先尝试 1 秒超时
    std::cout << "等待 1 秒...\n";
    bool completed = co_await wg->wait_for(1s, asio::use_awaitable);
    
    if (!completed) {
        std::cout << "⏱ 1 秒超时，剩余任务: " << wg->count() << "\n";
        
        // 继续等待
        std::cout << "继续等待...\n";
        completed = co_await wg->wait_for(5s, asio::use_awaitable);
        
        if (completed) {
            std::cout << "✓ 任务最终完成\n\n";
        }
    }
}

// 示例 3: 优雅关闭模拟
class SimpleServer {
    std::shared_ptr<acore::async_waitgroup> active_requests_;
    asio::io_context& io_context_;
    std::atomic<bool> shutting_down_{false};
    
public:
    SimpleServer(asio::io_context& io)
        : io_context_(io)
        , active_requests_(std::make_shared<acore::async_waitgroup>(io.get_executor()))
    {}
    
    void handle_request(int id, std::chrono::milliseconds duration) {
        if (shutting_down_) {
            std::cout << "  ✗ 拒绝新请求 " << id << "（正在关闭）\n";
            return;
        }
        
        active_requests_->add(1);
        
        asio::co_spawn(io_context_, 
            [this, id, duration]() -> asio::awaitable<void> {
                std::cout << "  → 请求 " << id << " 开始处理\n";
                
                asio::steady_timer timer(io_context_);
                timer.expires_after(duration);
                co_await timer.async_wait(asio::use_awaitable);
                
                std::cout << "  ✓ 请求 " << id << " 处理完成\n";
                active_requests_->done();
            }, 
            asio::detached
        );
    }
    
    asio::awaitable<void> shutdown() {
        std::cout << "\n开始优雅关闭...\n";
        shutting_down_ = true;
        
        int count = active_requests_->count();
        if (count > 0) {
            std::cout << "等待 " << count << " 个请求完成...\n";
            co_await active_requests_->wait(asio::use_awaitable);
        }
        
        std::cout << "✓ 服务器已安全关闭\n\n";
    }
};

asio::awaitable<void> example_graceful_shutdown() {
    auto ex = co_await asio::this_coro::executor;
    auto& io_context = static_cast<asio::io_context&>(ex.context());
    
    std::cout << "=== 示例 3: 优雅关闭 ===\n";
    
    SimpleServer server(io_context);
    
    // 模拟处理一些请求
    server.handle_request(1, 200ms);
    server.handle_request(2, 300ms);
    server.handle_request(3, 400ms);
    
    // 等待一会儿后开始关闭
    asio::steady_timer timer(io_context);
    timer.expires_after(150ms);
    co_await timer.async_wait(asio::use_awaitable);
    
    // 尝试添加新请求（会被拒绝）
    server.handle_request(4, 100ms);
    
    // 执行优雅关闭
    co_await server.shutdown();
}

int main() {
    try {
        asio::io_context io_context;
        
        asio::co_spawn(io_context, []() -> asio::awaitable<void> {
            co_await example_basic();
            co_await example_timeout();
            co_await example_graceful_shutdown();
            
            std::cout << "=================================\n";
            std::cout << "所有示例完成！\n";
            std::cout << "=================================\n";
        }, asio::detached);
        
        io_context.run();
        
    } catch (const std::exception& e) {
        std::cerr << "异常: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}

