//
// async_mutex 使用示例
//
#include "acore/async_mutex.hpp"
#include <asio.hpp>
#include <iostream>
#include <memory>

using namespace std::chrono_literals;
using asio::use_awaitable;

// 共享资源
struct SharedResource {
    int value = 0;
    std::shared_ptr<acore::async_mutex> mutex;
    
    explicit SharedResource(asio::any_io_executor ex)
        : mutex(std::make_shared<acore::async_mutex>(ex))
    {}
};

// 示例 1：RAII 风格使用（推荐）
asio::awaitable<void> example_raii_style() {
    std::cout << "\n=== 示例 1: RAII 风格（推荐）===\n";
    
    auto ex = co_await asio::this_coro::executor;
    auto resource = std::make_shared<SharedResource>(ex);
    
    // 多个协程并发修改
    for (int i = 0; i < 5; ++i) {
        asio::co_spawn(ex, [resource, i]() -> asio::awaitable<void> {
            // 自动获取锁
            auto guard = co_await resource->mutex->async_lock(use_awaitable);
            
            // 临界区：安全地修改共享资源
            int old_value = resource->value;
            co_await asio::post(asio::use_awaitable);  // 模拟异步操作
            resource->value = old_value + 1;
            
            std::cout << "Worker " << i << " updated value to " << resource->value << "\n";
            
            // guard 析构时自动释放锁
        }, asio::detached);
    }
    
    // 等待所有完成
    asio::steady_timer timer(ex);
    timer.expires_after(200ms);
    co_await timer.async_wait(use_awaitable);
    
    std::cout << "最终值: " << resource->value << " (期望: 5)\n";
}

// 示例 2：带超时的锁定
asio::awaitable<void> example_timeout() {
    std::cout << "\n=== 示例 2: 带超时的锁定 ===\n";
    
    auto ex = co_await asio::this_coro::executor;
    auto mutex = std::make_shared<acore::async_mutex>(ex);
    
    // 先获取锁
    co_await mutex->lock(use_awaitable);
    std::cout << "主协程获取锁\n";
    
    // 启动另一个协程尝试获取锁
    asio::co_spawn(ex, [mutex]() -> asio::awaitable<void> {
        std::cout << "尝试获取锁（超时 200ms）...\n";
        
        bool acquired = co_await mutex->try_lock_for(200ms, use_awaitable);
        
        if (acquired) {
            std::cout << "✓ 成功获取锁\n";
            mutex->unlock();
        } else {
            std::cout << "✗ 超时（按预期）\n";
        }
    }, asio::detached);
    
    // 等待一会儿
    asio::steady_timer timer(ex);
    timer.expires_after(300ms);
    co_await timer.async_wait(use_awaitable);
    
    // 释放锁
    mutex->unlock();
    std::cout << "主协程释放锁\n";
}

// 示例 3：连接池模式
class ConnectionPool {
private:
    std::vector<int> connections_;  // 模拟连接
    std::shared_ptr<acore::async_mutex> mutex_;
    
public:
    explicit ConnectionPool(asio::any_io_executor ex, int size)
        : mutex_(std::make_shared<acore::async_mutex>(ex))
    {
        for (int i = 0; i < size; ++i) {
            connections_.push_back(i);
        }
    }
    
    asio::awaitable<int> acquire() {
        auto guard = co_await mutex_->async_lock(use_awaitable);
        
        if (connections_.empty()) {
            throw std::runtime_error("No available connections");
        }
        
        int conn = connections_.back();
        connections_.pop_back();
        
        co_return conn;
    }
    
    asio::awaitable<void> release(int conn) {
        auto guard = co_await mutex_->async_lock(use_awaitable);
        connections_.push_back(conn);
    }
    
    asio::awaitable<size_t> available_count() {
        auto guard = co_await mutex_->async_lock(use_awaitable);
        co_return connections_.size();
    }
};

asio::awaitable<void> example_connection_pool() {
    std::cout << "\n=== 示例 3: 连接池模式 ===\n";
    
    auto ex = co_await asio::this_coro::executor;
    auto pool = std::make_shared<ConnectionPool>(ex, 3);
    
    std::cout << "连接池大小: 3\n";
    
    // 多个协程使用连接池
    for (int i = 0; i < 5; ++i) {
        asio::co_spawn(ex, [pool, i]() -> asio::awaitable<void> {
            try {
                int conn = co_await pool->acquire();
                std::cout << "Worker " << i << " acquired connection " << conn << "\n";
                
                // 使用连接
                asio::steady_timer timer(co_await asio::this_coro::executor);
                timer.expires_after(100ms);
                co_await timer.async_wait(use_awaitable);
                
                // 归还连接
                co_await pool->release(conn);
                std::cout << "Worker " << i << " released connection " << conn << "\n";
            } catch (const std::exception& e) {
                std::cout << "Worker " << i << " error: " << e.what() << "\n";
            }
        }, asio::detached);
    }
    
    asio::steady_timer timer(ex);
    timer.expires_after(1s);
    co_await timer.async_wait(use_awaitable);
}

// 主函数
asio::awaitable<void> run_examples() {
    co_await example_raii_style();
    co_await example_timeout();
    co_await example_connection_pool();
    
    std::cout << "\n✅ 所有示例完成\n";
}

int main() {
    asio::io_context io_context;
    asio::co_spawn(io_context, run_examples(), asio::detached);
    io_context.run();
    return 0;
}

