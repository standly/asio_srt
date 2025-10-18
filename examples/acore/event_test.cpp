//
// Test async_event implementation
//

#include "acore/async_event.hpp"
#include <asio.hpp>
#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/use_awaitable.hpp>
#include <iostream>
#include <chrono>

using asio::awaitable;
using asio::co_spawn;
using asio::detached;
using asio::use_awaitable;
using namespace std::chrono_literals;

// 测试1：基本的等待和触发
awaitable<void> test_basic(asio::io_context& io) {
    std::cout << "\n=== Test 1: Basic wait/notify ===" << std::endl;
    
    acore::async_event event(io.get_executor());
    
    // 启动等待者
    co_spawn(io, [&event]() -> awaitable<void> {
        std::cout << "[Waiter] Waiting for event..." << std::endl;
        co_await event.wait(use_awaitable);
        std::cout << "[Waiter] Event received! ✅" << std::endl;
    }, detached);
    
    // 延迟触发
    auto executor = co_await asio::this_coro::executor;
    asio::steady_timer timer(executor);
    timer.expires_after(100ms);
    co_await timer.async_wait(use_awaitable);
    
    std::cout << "[Main] Triggering event (notify_all)..." << std::endl;
    event.notify_all();
    
    // 等待完成
    timer.expires_after(100ms);
    co_await timer.async_wait(use_awaitable);
}

// 测试2：广播 - 唤醒所有等待者
awaitable<void> test_broadcast(asio::io_context& io) {
    std::cout << "\n=== Test 2: Broadcast (notify_all) ===" << std::endl;
    
    acore::async_event event(io.get_executor());
    
    // 启动多个等待者
    for (int i = 1; i <= 5; ++i) {
        co_spawn(io, [&event, i]() -> awaitable<void> {
            std::cout << "[Waiter " << i << "] Waiting..." << std::endl;
            co_await event.wait(use_awaitable);
            std::cout << "[Waiter " << i << "] Received! ✅" << std::endl;
        }, detached);
    }
    
    auto executor = co_await asio::this_coro::executor;
    asio::steady_timer timer(executor);
    timer.expires_after(100ms);
    co_await timer.async_wait(use_awaitable);
    
    std::cout << "[Main] Broadcasting event..." << std::endl;
    event.notify_all();
    
    timer.expires_after(200ms);
    co_await timer.async_wait(use_awaitable);
    
    std::cout << "[Main] Expected: All 5 waiters received the event ✅" << std::endl;
}

// 测试3：事件已触发时立即返回
awaitable<void> test_already_set(asio::io_context& io) {
    std::cout << "\n=== Test 3: Event already set ===" << std::endl;
    
    acore::async_event event(io.get_executor());
    
    std::cout << "[Main] Triggering event first..." << std::endl;
    event.notify_all();
    
    auto executor = co_await asio::this_coro::executor;
    asio::steady_timer timer(executor);
    timer.expires_after(100ms);
    co_await timer.async_wait(use_awaitable);
    
    // 事件已触发，应该立即返回
    std::cout << "[Waiter] Waiting on already-set event..." << std::endl;
    co_await event.wait(use_awaitable);
    std::cout << "[Waiter] Returned immediately! ✅" << std::endl;
}

// 测试4：手动重置
awaitable<void> test_reset(asio::io_context& io) {
    std::cout << "\n=== Test 4: Manual reset ===" << std::endl;
    
    acore::async_event event(io.get_executor());
    
    // 第一次触发
    std::cout << "[Main] First trigger..." << std::endl;
    event.notify_all();
    
    auto executor = co_await asio::this_coro::executor;
    asio::steady_timer timer(executor);
    timer.expires_after(100ms);
    co_await timer.async_wait(use_awaitable);
    
    co_await event.wait(use_awaitable);
    std::cout << "[Main] First wait completed ✅" << std::endl;
    
    // 重置
    std::cout << "[Main] Resetting event..." << std::endl;
    event.reset();
    
    timer.expires_after(100ms);
    co_await timer.async_wait(use_awaitable);
    
    // 启动新等待者
    co_spawn(io, [&event]() -> awaitable<void> {
        std::cout << "[Waiter] Waiting after reset..." << std::endl;
        co_await event.wait(use_awaitable);
        std::cout << "[Waiter] Received after reset! ✅" << std::endl;
    }, detached);
    
    timer.expires_after(100ms);
    co_await timer.async_wait(use_awaitable);
    
    // 再次触发
    std::cout << "[Main] Triggering again..." << std::endl;
    event.notify_all();
    
    timer.expires_after(100ms);
    co_await timer.async_wait(use_awaitable);
}

// 测试5：超时 - 事件在超时前触发
awaitable<void> test_timeout_triggered(asio::io_context& io) {
    std::cout << "\n=== Test 5: Timeout - event triggered in time ===" << std::endl;
    
    acore::async_event event(io.get_executor());
    
    // 启动等待者（2秒超时）
    co_spawn(io, [&event]() -> awaitable<void> {
        std::cout << "[Waiter] Waiting with 2s timeout..." << std::endl;
        bool triggered = co_await event.wait_for(2s, use_awaitable);
        if (triggered) {
            std::cout << "[Waiter] Event triggered in time! ✅" << std::endl;
        } else {
            std::cout << "[Waiter] Timeout! ❌" << std::endl;
        }
    }, detached);
    
    // 500ms 后触发（在超时前）
    auto executor = co_await asio::this_coro::executor;
    asio::steady_timer timer(executor);
    timer.expires_after(500ms);
    co_await timer.async_wait(use_awaitable);
    
    std::cout << "[Main] Triggering event (before timeout)..." << std::endl;
    event.notify_all();
    
    timer.expires_after(200ms);
    co_await timer.async_wait(use_awaitable);
}

// 测试6：超时 - 事件超时
awaitable<void> test_timeout_expired(asio::io_context& io) {
    std::cout << "\n=== Test 6: Timeout - event timeout ===" << std::endl;
    
    acore::async_event event(io.get_executor());
    
    // 启动等待者（500ms 超时）
    co_spawn(io, [&event]() -> awaitable<void> {
        std::cout << "[Waiter] Waiting with 500ms timeout..." << std::endl;
        bool triggered = co_await event.wait_for(500ms, use_awaitable);
        if (triggered) {
            std::cout << "[Waiter] Event triggered! ❌" << std::endl;
        } else {
            std::cout << "[Waiter] Timeout as expected! ✅" << std::endl;
        }
    }, detached);
    
    // 不触发事件，等待超时
    auto executor = co_await asio::this_coro::executor;
    asio::steady_timer timer(executor);
    timer.expires_after(800ms);
    co_await timer.async_wait(use_awaitable);
    
    std::cout << "[Main] Event was not triggered (timeout expected)" << std::endl;
}

// 测试7：多个等待者，部分超时
awaitable<void> test_mixed_timeout(asio::io_context& io) {
    std::cout << "\n=== Test 7: Mixed timeout (some timeout, some triggered) ===" << std::endl;
    
    acore::async_event event(io.get_executor());
    
    // 短超时等待者（500ms）
    co_spawn(io, [&event]() -> awaitable<void> {
        std::cout << "[Waiter 1] Waiting with 500ms timeout..." << std::endl;
        bool triggered = co_await event.wait_for(500ms, use_awaitable);
        std::cout << "[Waiter 1] " << (triggered ? "Triggered ❌" : "Timeout ✅") << std::endl;
    }, detached);
    
    // 长超时等待者（2s）
    co_spawn(io, [&event]() -> awaitable<void> {
        std::cout << "[Waiter 2] Waiting with 2s timeout..." << std::endl;
        bool triggered = co_await event.wait_for(2s, use_awaitable);
        std::cout << "[Waiter 2] " << (triggered ? "Triggered ✅" : "Timeout ❌") << std::endl;
    }, detached);
    
    // 无超时等待者
    co_spawn(io, [&event]() -> awaitable<void> {
        std::cout << "[Waiter 3] Waiting without timeout..." << std::endl;
        co_await event.wait(use_awaitable);
        std::cout << "[Waiter 3] Triggered ✅" << std::endl;
    }, detached);
    
    // 1秒后触发
    auto executor = co_await asio::this_coro::executor;
    asio::steady_timer timer(executor);
    timer.expires_after(1s);
    co_await timer.async_wait(use_awaitable);
    
    std::cout << "[Main] Triggering event (after 1s)..." << std::endl;
    event.notify_all();
    
    timer.expires_after(200ms);
    co_await timer.async_wait(use_awaitable);
    
    std::cout << "[Main] Expected: Waiter 1 timeout, Waiter 2 & 3 triggered" << std::endl;
}

// 测试8：压力测试 - 状态同步
awaitable<void> test_state_sync(asio::io_context& io) {
    std::cout << "\n=== Test 8: State synchronization stress test ===" << std::endl;
    
    acore::async_event event(io.get_executor());
    auto count = std::make_shared<std::atomic<int>>(0);
    
    // 100 个等待者
    for (int i = 1; i <= 100; ++i) {
        co_spawn(io, [&event, count]() -> awaitable<void> {
            co_await event.wait(use_awaitable);
            count->fetch_add(1);
        }, detached);
    }
    
    auto executor = co_await asio::this_coro::executor;
    asio::steady_timer timer(executor);
    timer.expires_after(100ms);
    co_await timer.async_wait(use_awaitable);
    
    std::cout << "[Main] Broadcasting to 100 waiters..." << std::endl;
    event.notify_all();
    
    timer.expires_after(500ms);
    co_await timer.async_wait(use_awaitable);
    
    std::cout << "[Main] Notified count: " << count->load() 
              << " (expected: 100)" << std::endl;
    
    if (count->load() == 100) {
        std::cout << "✅ State synchronization test PASSED" << std::endl;
    } else {
        std::cout << "❌ State synchronization test FAILED" << std::endl;
    }
}

int main() {
    try {
        std::cout << "Async Event Test Suite" << std::endl;
        std::cout << "======================" << std::endl;
        
        asio::io_context io;
        
        // 运行所有测试
        co_spawn(io, [&io]() -> awaitable<void> {
            co_await test_basic(io);
            co_await test_broadcast(io);
            co_await test_already_set(io);
            co_await test_reset(io);
            co_await test_timeout_triggered(io);
            co_await test_timeout_expired(io);
            co_await test_mixed_timeout(io);
            co_await test_state_sync(io);
            
            std::cout << "\n======================" << std::endl;
            std::cout << "All tests completed!" << std::endl;
        }, detached);
        
        io.run();
        
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}


