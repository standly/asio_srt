//
// 测试 async_waitgroup
//
#include "acore/async_waitgroup.hpp"
#include <iostream>
#include <chrono>
#include <string>

using namespace std::chrono_literals;

// 测试 1: 基本功能 - 等待多个任务完成
asio::awaitable<void> test_basic_usage() {
    auto ex = co_await asio::this_coro::executor;
    auto& io_context = static_cast<asio::io_context&>(ex.context());
    
    auto wg = std::make_shared<acore::async_waitgroup>(ex);
    
    std::cout << "测试 1: 基本功能 - 等待多个任务完成\n";
    std::cout << "  → 启动 5 个异步任务...\n";
    
    // 启动 5 个异步任务
    wg->add(5);
    for (int i = 0; i < 5; ++i) {
        asio::co_spawn(ex, [wg, i, &io_context]() -> asio::awaitable<void> {
            // 模拟异步工作
            asio::steady_timer timer(io_context);
            timer.expires_after(std::chrono::milliseconds(100 * (i + 1)));
            co_await timer.async_wait(asio::use_awaitable);
            
            std::cout << "    任务 " << i << " 完成\n";
            wg->done();
        }, asio::detached);
    }
    
    // 等待所有任务完成
    std::cout << "  → 等待所有任务完成...\n";
    auto start = std::chrono::steady_clock::now();
    co_await wg->wait(asio::use_awaitable);
    auto duration = std::chrono::steady_clock::now() - start;
    
    std::cout << "  ✓ 所有任务完成！耗时: " 
              << std::chrono::duration_cast<std::chrono::milliseconds>(duration).count() 
              << " ms\n";
    std::cout << "  ✓ 当前计数: " << wg->count() << "\n\n";
}

// 测试 2: 批量添加和提前完成
asio::awaitable<void> test_batch_add() {
    auto ex = co_await asio::this_coro::executor;
    auto& io_context = static_cast<asio::io_context&>(ex.context());
    
    auto wg = std::make_shared<acore::async_waitgroup>(ex);
    
    std::cout << "测试 2: 批量添加和快速完成\n";
    std::cout << "  → 批量添加 10 个任务\n";
    
    // 批量添加
    wg->add(10);
    std::cout << "  → 当前计数: " << wg->count() << "\n";
    
    // 快速完成所有任务
    for (int i = 0; i < 10; ++i) {
        asio::co_spawn(ex, [wg, i]() -> asio::awaitable<void> {
            std::cout << "    任务 " << i << " 立即完成\n";
            wg->done();
            co_return;
        }, asio::detached);
    }
    
    // 等待完成
    co_await wg->wait(asio::use_awaitable);
    std::cout << "  ✓ 批量任务全部完成\n";
    std::cout << "  ✓ 最终计数: " << wg->count() << "\n\n";
}

// 测试 3: 超时等待
asio::awaitable<void> test_timeout() {
    auto ex = co_await asio::this_coro::executor;
    auto& io_context = static_cast<asio::io_context&>(ex.context());
    
    auto wg = std::make_shared<acore::async_waitgroup>(ex);
    
    std::cout << "测试 3: 超时等待\n";
    std::cout << "  → 启动一个需要 3 秒的任务\n";
    
    wg->add(1);
    asio::co_spawn(ex, [wg, &io_context]() -> asio::awaitable<void> {
        asio::steady_timer timer(io_context);
        timer.expires_after(3s);
        co_await timer.async_wait(asio::use_awaitable);
        std::cout << "    慢任务完成\n";
        wg->done();
    }, asio::detached);
    
    // 先尝试 1 秒超时
    std::cout << "  → 等待 1 秒...\n";
    bool completed = co_await wg->wait_for(1s, asio::use_awaitable);
    
    if (!completed) {
        std::cout << "  ✓ 1 秒超时（预期）\n";
        std::cout << "  → 当前计数: " << wg->count() << "\n";
    } else {
        std::cout << "  ✗ 不应该在 1 秒内完成\n";
    }
    
    // 再等待足够长的时间
    std::cout << "  → 继续等待 5 秒...\n";
    completed = co_await wg->wait_for(5s, asio::use_awaitable);
    
    if (completed) {
        std::cout << "  ✓ 任务最终完成\n";
        std::cout << "  ✓ 最终计数: " << wg->count() << "\n";
    } else {
        std::cout << "  ✗ 不应该超时\n";
    }
    
    std::cout << "\n";
}

// 测试 4: 多个等待者
asio::awaitable<void> test_multiple_waiters() {
    auto ex = co_await asio::this_coro::executor;
    auto& io_context = static_cast<asio::io_context&>(ex.context());
    
    auto wg = std::make_shared<acore::async_waitgroup>(ex);
    
    std::cout << "测试 4: 多个等待者\n";
    std::cout << "  → 启动 3 个等待者\n";
    
    int notified_count = 0;
    
    // 启动 3 个等待者
    for (int i = 0; i < 3; ++i) {
        asio::co_spawn(ex, [wg, i, &notified_count]() -> asio::awaitable<void> {
            co_await wg->wait(asio::use_awaitable);
            ++notified_count;
            std::cout << "    等待者 " << i << " 被唤醒\n";
        }, asio::detached);
    }
    
    // 给等待者一点时间来注册
    asio::steady_timer timer(io_context, 100ms);
    co_await timer.async_wait(asio::use_awaitable);
    
    // 启动并完成一个任务
    std::cout << "  → 启动并完成 1 个任务\n";
    wg->add(1);
    wg->done();
    
    // 等待所有等待者被唤醒
    timer.expires_after(100ms);
    co_await timer.async_wait(asio::use_awaitable);
    
    if (notified_count == 3) {
        std::cout << "  ✓ 所有等待者都被唤醒\n";
    } else {
        std::cout << "  ✗ 只有 " << notified_count << " 个等待者被唤醒\n";
    }
    
    std::cout << "\n";
}

// 测试 5: 立即完成（计数已为 0）
asio::awaitable<void> test_immediate_completion() {
    auto ex = co_await asio::this_coro::executor;
    
    auto wg = std::make_shared<acore::async_waitgroup>(ex);
    
    std::cout << "测试 5: 立即完成（计数已为 0）\n";
    std::cout << "  → 当前计数: " << wg->count() << "\n";
    std::cout << "  → 调用 wait()...\n";
    
    auto start = std::chrono::steady_clock::now();
    co_await wg->wait(asio::use_awaitable);
    auto duration = std::chrono::steady_clock::now() - start;
    
    std::cout << "  ✓ 立即完成！耗时: " 
              << std::chrono::duration_cast<std::chrono::microseconds>(duration).count() 
              << " μs\n\n";
}

// 测试 6: 嵌套使用 - 等待子任务组
asio::awaitable<void> test_nested_waitgroups() {
    auto ex = co_await asio::this_coro::executor;
    auto& io_context = static_cast<asio::io_context&>(ex.context());
    
    auto main_wg = std::make_shared<acore::async_waitgroup>(ex);
    
    std::cout << "测试 6: 嵌套使用 - 等待子任务组\n";
    std::cout << "  → 启动 3 个主任务，每个主任务有 3 个子任务\n";
    
    main_wg->add(3);
    
    for (int i = 0; i < 3; ++i) {
        asio::co_spawn(ex, [main_wg, i, &io_context, ex]() -> asio::awaitable<void> {
            std::cout << "    主任务 " << i << " 开始\n";
            
            // 创建子任务组
            auto sub_wg = std::make_shared<acore::async_waitgroup>(ex);
            sub_wg->add(3);
            
            for (int j = 0; j < 3; ++j) {
                asio::co_spawn(ex, [sub_wg, i, j, &io_context]() -> asio::awaitable<void> {
                    asio::steady_timer timer(io_context);
                    timer.expires_after(std::chrono::milliseconds(50));
                    co_await timer.async_wait(asio::use_awaitable);
                    std::cout << "      子任务 " << i << "." << j << " 完成\n";
                    sub_wg->done();
                }, asio::detached);
            }
            
            // 等待所有子任务完成
            co_await sub_wg->wait(asio::use_awaitable);
            std::cout << "    主任务 " << i << " 完成（所有子任务完成）\n";
            
            main_wg->done();
        }, asio::detached);
    }
    
    // 等待所有主任务完成
    std::cout << "  → 等待所有主任务完成...\n";
    co_await main_wg->wait(asio::use_awaitable);
    std::cout << "  ✓ 所有主任务和子任务都完成\n\n";
}

// 测试 7: 使用 RAII 风格的自动 done()
class WaitGroupGuard {
public:
    explicit WaitGroupGuard(std::shared_ptr<acore::async_waitgroup> wg)
        : wg_(std::move(wg)) {}
    
    ~WaitGroupGuard() {
        if (wg_) {
            wg_->done();
        }
    }
    
    // 禁止复制，允许移动
    WaitGroupGuard(const WaitGroupGuard&) = delete;
    WaitGroupGuard& operator=(const WaitGroupGuard&) = delete;
    WaitGroupGuard(WaitGroupGuard&& other) noexcept : wg_(std::move(other.wg_)) {}
    WaitGroupGuard& operator=(WaitGroupGuard&&) = delete;
    
private:
    std::shared_ptr<acore::async_waitgroup> wg_;
};

asio::awaitable<void> test_raii_guard() {
    auto ex = co_await asio::this_coro::executor;
    auto& io_context = static_cast<asio::io_context&>(ex.context());
    
    auto wg = std::make_shared<acore::async_waitgroup>(ex);
    
    std::cout << "测试 7: RAII 风格的自动 done()\n";
    std::cout << "  → 启动 3 个使用 guard 的任务\n";
    
    wg->add(3);
    
    for (int i = 0; i < 3; ++i) {
        asio::co_spawn(ex, [wg, i, &io_context]() -> asio::awaitable<void> {
            WaitGroupGuard guard(wg);  // 析构时自动调用 done()
            
            asio::steady_timer timer(io_context);
            timer.expires_after(std::chrono::milliseconds(100));
            co_await timer.async_wait(asio::use_awaitable);
            
            std::cout << "    任务 " << i << " 完成（guard 会自动调用 done）\n";
            // guard 析构，自动调用 wg->done()
        }, asio::detached);
    }
    
    co_await wg->wait(asio::use_awaitable);
    std::cout << "  ✓ 所有任务完成（通过 RAII guard）\n\n";
}

int main() {
    try {
        asio::io_context io_context;
        
        asio::co_spawn(io_context, []() -> asio::awaitable<void> {
            co_await test_basic_usage();
            co_await test_batch_add();
            co_await test_timeout();
            co_await test_multiple_waiters();
            co_await test_immediate_completion();
            co_await test_nested_waitgroups();
            co_await test_raii_guard();
            
            std::cout << "=================================\n";
            std::cout << "所有测试完成！✓\n";
            std::cout << "=================================\n";
        }, asio::detached);
        
        io_context.run();
        
    } catch (const std::exception& e) {
        std::cerr << "异常: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}



