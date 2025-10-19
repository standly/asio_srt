//
// dispatcher 全面测试 - 包括竞态条件测试
//
#include "acore/dispatcher.hpp"
#include <iostream>
#include <chrono>
#include <atomic>
#include <set>

using namespace std::chrono_literals;

// 测试 1: 基本发布订阅
asio::awaitable<void> test_basic_pubsub() {
    auto ex = co_await asio::this_coro::executor;
    auto& io_context = static_cast<asio::io_context&>(ex.context());
    
    auto disp = acore::make_dispatcher<int>(io_context);
    
    std::cout << "测试 1: 基本发布订阅\n";
    
    auto queue = disp->subscribe();
    
    // 等待订阅完成
    asio::steady_timer timer(ex, 50ms);
    co_await timer.async_wait(asio::use_awaitable);
    
    // 发布 3 条消息
    std::cout << "  → 发布 3 条消息...\n";
    disp->publish(1);
    disp->publish(2);
    disp->publish(3);
    
    // 读取 3 条消息
    for (int i = 1; i <= 3; ++i) {
        try {
            int msg = co_await queue->async_read_msg(asio::use_awaitable);
            if (msg == i) {
                std::cout << "  ✓ 接收消息: " << msg << "\n";
            } else {
                std::cout << "  ✗ 期望 " << i << "，得到 " << msg << "\n";
            }
        } catch (const std::system_error& e) {
            std::cout << "  ✗ 读取失败: " << e.what() << "\n";
        }
    }
    
    std::cout << "\n";
}

// 测试 2: 多个订阅者
asio::awaitable<void> test_multiple_subscribers() {
    auto ex = co_await asio::this_coro::executor;
    auto& io_context = static_cast<asio::io_context&>(ex.context());
    
    auto disp = acore::make_dispatcher<int>(io_context);
    
    std::cout << "测试 2: 多个订阅者（广播测试）\n";
    
    const int num_subscribers = 5;
    std::vector<std::shared_ptr<acore::async_queue<int>>> queues;
    
    // 创建 5 个订阅者
    std::cout << "  → 创建 " << num_subscribers << " 个订阅者...\n";
    for (int i = 0; i < num_subscribers; ++i) {
        queues.push_back(disp->subscribe());
    }
    
    // 等待订阅完成
    asio::steady_timer timer(ex, 100ms);
    co_await timer.async_wait(asio::use_awaitable);
    
    // 发布 3 条消息
    std::cout << "  → 发布 3 条消息...\n";
    disp->publish(100);
    disp->publish(200);
    disp->publish(300);
    
    // 每个订阅者读取
    timer.expires_after(100ms);
    co_await timer.async_wait(asio::use_awaitable);
    
    int correct_subscribers = 0;
    for (int s = 0; s < num_subscribers; ++s) {
        std::vector<int> received;
        
        for (int m = 0; m < 3; ++m) {
            try {
                int msg = co_await queues[s]->async_read_msg(asio::use_awaitable);
                received.push_back(msg);
            } catch (...) {
                break;
            }
        }
        
        if (received.size() == 3 && received[0] == 100 && received[1] == 200 && received[2] == 300) {
            correct_subscribers++;
        } else {
            std::cout << "  ✗ 订阅者 " << s << " 接收不正确\n";
        }
    }
    
    if (correct_subscribers == num_subscribers) {
        std::cout << "  ✓ 所有 " << num_subscribers << " 个订阅者都接收了全部消息\n";
    }
    
    std::cout << "\n";
}

// 测试 3: 订阅的竞态 - 订阅后立即发布
asio::awaitable<void> test_subscribe_timing() {
    auto ex = co_await asio::this_coro::executor;
    auto& io_context = static_cast<asio::io_context&>(ex.context());
    
    auto disp = acore::make_dispatcher<int>(io_context);
    
    std::cout << "测试 3: 订阅时序测试（竞态测试）\n";
    std::cout << "  → 测试订阅后立即发布可能错过消息...\n";
    
    int received_immediate = 0;
    int received_delayed = 0;
    
    // 测试 A: 立即发布（可能错过）
    auto queue1 = disp->subscribe();
    disp->publish(1);  // 立即发布
    
    asio::steady_timer timer(ex, 50ms);
    co_await timer.async_wait(asio::use_awaitable);
    
    try {
        int msg = co_await queue1->async_read_msg(asio::use_awaitable);
        received_immediate = 1;
        std::cout << "  ✓ 立即订阅接收了消息（运气好）\n";
    } catch (...) {
        std::cout << "  ⚠ 立即订阅错过了消息（预期行为，订阅是异步的）\n";
    }
    
    // 测试 B: 等待后发布（应该接收）
    auto queue2 = disp->subscribe();
    
    // 等待订阅完成
    size_t count = co_await disp->async_subscriber_count(asio::use_awaitable);
    std::cout << "  → 确认订阅完成，订阅者数量: " << count << "\n";
    
    disp->publish(2);
    
    timer.expires_after(50ms);
    co_await timer.async_wait(asio::use_awaitable);
    
    try {
        int msg = co_await queue2->async_read_msg(asio::use_awaitable);
        if (msg == 2) {
            received_delayed = 1;
            std::cout << "  ✓ 等待后订阅接收了消息\n";
        }
    } catch (...) {
        std::cout << "  ✗ 等待后订阅也错过了消息\n";
    }
    
    if (received_delayed == 1) {
        std::cout << "  ✓ 使用 async_subscriber_count() 确保订阅完成可避免错过消息\n";
    }
    
    std::cout << "\n";
}

// 测试 4: 大量订阅者的性能
asio::awaitable<void> test_many_subscribers() {
    auto ex = co_await asio::this_coro::executor;
    auto& io_context = static_cast<asio::io_context&>(ex.context());
    
    auto disp = acore::make_dispatcher<int>(io_context);
    
    std::cout << "测试 4: 大量订阅者（性能测试）\n";
    
    const int num_subscribers = 100;
    std::vector<std::shared_ptr<acore::async_queue<int>>> queues;
    
    std::cout << "  → 创建 " << num_subscribers << " 个订阅者...\n";
    for (int i = 0; i < num_subscribers; ++i) {
        queues.push_back(disp->subscribe());
    }
    
    // 等待所有订阅完成
    asio::steady_timer timer(ex, 100ms);
    co_await timer.async_wait(asio::use_awaitable);
    
    // 发布 10 条消息
    std::cout << "  → 发布 10 条消息...\n";
    auto start = std::chrono::steady_clock::now();
    
    for (int m = 0; m < 10; ++m) {
        disp->publish(m);
    }
    
    auto publish_duration = std::chrono::steady_clock::now() - start;
    
    // 等待消息传播
    timer.expires_after(200ms);
    co_await timer.async_wait(asio::use_awaitable);
    
    // 随机抽查 5 个订阅者
    int correct = 0;
    for (int s = 0; s < 5; ++s) {
        int idx = s * 20;  // 每隔 20 个抽一个
        int received = 0;
        
        for (int m = 0; m < 10; ++m) {
            try {
                int msg = co_await queues[idx]->async_read_msg(asio::use_awaitable);
                received++;
            } catch (...) {
                break;
            }
        }
        
        if (received == 10) {
            correct++;
        }
    }
    
    std::cout << "  → 发布耗时: " 
              << std::chrono::duration_cast<std::chrono::microseconds>(publish_duration).count()
              << " μs\n";
    
    if (correct == 5) {
        std::cout << "  ✓ 抽查的 5 个订阅者都接收了全部消息\n";
    } else {
        std::cout << "  ⚠ 只有 " << correct << " 个订阅者正确\n";
    }
    
    std::cout << "\n";
}

// 测试 5: 并发订阅和取消订阅
asio::awaitable<void> test_concurrent_subscribe_unsubscribe() {
    auto ex = co_await asio::this_coro::executor;
    auto& io_context = static_cast<asio::io_context&>(ex.context());
    
    auto disp = acore::make_dispatcher<int>(io_context);
    
    std::cout << "测试 5: 并发订阅/取消订阅（竞态测试）\n";
    std::cout << "  → 同时订阅、发布、取消订阅...\n";
    
    std::vector<uint64_t> subscriber_ids;
    std::atomic<int> total_received{0};
    
    // 动态订阅和取消
    for (int round = 0; round < 5; ++round) {
        // 添加 10 个订阅者
        for (int i = 0; i < 10; ++i) {
            auto [id, queue] = disp->subscribe_with_id();
            subscriber_ids.push_back(id);
            
            // 启动读取
            asio::co_spawn(ex, [queue, &total_received]() -> asio::awaitable<void> {
                while (true) {
                    try {
                        int msg = co_await queue->async_read_msg(asio::use_awaitable);
                        total_received.fetch_add(1, std::memory_order_relaxed);
                    } catch (...) {
                        break;
                    }
                }
            }, asio::detached);
        }
        
        // 发布一些消息
        for (int m = 0; m < 5; ++m) {
            disp->publish(round * 100 + m);
        }
        
        // 取消一半订阅者
        for (int i = 0; i < 5; ++i) {
            if (!subscriber_ids.empty()) {
                disp->unsubscribe(subscriber_ids.back());
                subscriber_ids.pop_back();
            }
        }
    }
    
    // 等待完成
    asio::steady_timer timer(ex, 300ms);
    co_await timer.async_wait(asio::use_awaitable);
    
    // 清理剩余订阅者
    disp->clear();
    
    timer.expires_after(100ms);
    co_await timer.async_wait(asio::use_awaitable);
    
    std::cout << "  → 总共接收: " << total_received.load() << " 条消息\n";
    std::cout << "  ✓ 并发订阅/取消订阅测试完成（无crash）\n";
    
    std::cout << "\n";
}

int main() {
    try {
        asio::io_context io_context;
        
        asio::co_spawn(io_context, []() -> asio::awaitable<void> {
            co_await test_basic_pubsub();
            co_await test_multiple_subscribers();
            co_await test_subscribe_timing();
            co_await test_many_subscribers();
            co_await test_concurrent_subscribe_unsubscribe();
            
            std::cout << "=================================\n";
            std::cout << "dispatcher 所有测试完成！✓\n";
            std::cout << "=================================\n";
        }, asio::detached);
        
        io_context.run();
        
    } catch (const std::exception& e) {
        std::cerr << "异常: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}

