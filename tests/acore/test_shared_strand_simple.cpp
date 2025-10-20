//
// 共享 Strand 简化单元测试
//
// 测试核心 ACORE 组件在共享 strand 情况下的正确性
//

#include "acore/async_mutex.hpp"
#include "acore/async_queue.hpp"
#include "acore/async_semaphore.hpp"

#include <asio.hpp>
#include <gtest/gtest.h>
#include <atomic>

using namespace acore;
using namespace std::chrono_literals;

// ============================================================================
// 测试 1: 两个 mutex 共享 strand
// ============================================================================

TEST(SharedStrandSimpleTest, TwoMutexesSharedStrand) {
    asio::io_context io_context;
    asio::strand<asio::any_io_executor> shared_strand = asio::make_strand(io_context.get_executor());
    
    auto mutex1 = std::make_shared<async_mutex>(shared_strand);
    auto mutex2 = std::make_shared<async_mutex>(shared_strand);
    
    std::atomic<int> counter{0};
    std::atomic<bool> done{false};
    
    // 启动协程：锁定两个 mutex
    asio::co_spawn(shared_strand, [&]() -> asio::awaitable<void> {
        // 锁定 mutex1
        auto guard1 = co_await mutex1->async_lock(asio::use_awaitable);
        counter++;
        
        // 锁定 mutex2
        auto guard2 = co_await mutex2->async_lock(asio::use_awaitable);
        counter++;
        
        // 释放
        guard2.unlock();
        guard1.unlock();
        
        done = true;
    }, asio::detached);
    
    io_context.run();
    
    EXPECT_TRUE(done);
    EXPECT_EQ(counter, 2);
}

// ============================================================================
// 测试 2: mutex + queue 共享 strand
// ============================================================================

TEST(SharedStrandSimpleTest, MutexAndQueueSharedStrand) {
    asio::io_context io_context;
    asio::strand<asio::any_io_executor> shared_strand = asio::make_strand(io_context.get_executor());
    
    auto mutex = std::make_shared<async_mutex>(shared_strand);
    auto queue = std::make_shared<async_queue<int>>(io_context, shared_strand);
    
    std::atomic<int> received_value{0};
    std::atomic<bool> done{false};
    
    // 生产者
    asio::co_spawn(shared_strand, [&]() -> asio::awaitable<void> {
        auto guard = co_await mutex->async_lock(asio::use_awaitable);
        queue->push(42);
        co_return;
    }, asio::detached);
    
    // 消费者
    asio::co_spawn(shared_strand, [&]() -> asio::awaitable<void> {
        auto value = co_await queue->async_read_msg(asio::use_awaitable);
        
        auto guard = co_await mutex->async_lock(asio::use_awaitable);
        received_value = value;
        done = true;
    }, asio::detached);
    
    io_context.run();
    
    EXPECT_TRUE(done);
    EXPECT_EQ(received_value, 42);
}

// ============================================================================
// 测试 3: semaphore 共享 strand
// ============================================================================

TEST(SharedStrandSimpleTest, SemaphoresSharedStrand) {
    asio::io_context io_context;
    asio::strand<asio::any_io_executor> shared_strand = asio::make_strand(io_context.get_executor());
    
    auto sem1 = std::make_shared<async_semaphore>(shared_strand, 2);
    auto sem2 = std::make_shared<async_semaphore>(shared_strand, 1);
    
    std::atomic<int> acquired_count{0};
    
    // 任务 1
    asio::co_spawn(shared_strand, [&]() -> asio::awaitable<void> {
        co_await sem1->acquire(asio::use_awaitable);
        acquired_count++;
        co_await sem2->acquire(asio::use_awaitable);
        acquired_count++;
        
        sem2->release();
        sem1->release();
    }, asio::detached);
    
    // 任务 2
    asio::co_spawn(shared_strand, [&]() -> asio::awaitable<void> {
        co_await sem1->acquire(asio::use_awaitable);
        acquired_count++;
        
        sem1->release();
    }, asio::detached);
    
    io_context.run();
    
    EXPECT_EQ(acquired_count, 3);
}

// ============================================================================
// 测试 4: 复杂协作 - 多个组件
// ============================================================================

TEST(SharedStrandSimpleTest, ComplexCollaboration) {
    asio::io_context io_context;
    asio::strand<asio::any_io_executor> shared_strand = asio::make_strand(io_context.get_executor());
    
    // 创建多个组件，全部共享 strand
    auto mutex = std::make_shared<async_mutex>(shared_strand);
    auto queue = std::make_shared<async_queue<std::string>>(io_context, shared_strand);
    auto semaphore = std::make_shared<async_semaphore>(shared_strand, 5);
    
    std::vector<std::string> received_messages;
    std::atomic<bool> producer_done{false};
    std::atomic<bool> consumer_done{false};
    
    // 生产者
    asio::co_spawn(shared_strand, [&]() -> asio::awaitable<void> {
        for (int i = 0; i < 5; ++i) {
            // 获取信号量
            co_await semaphore->acquire(asio::use_awaitable);
            
            // 锁定
            auto guard = co_await mutex->async_lock(asio::use_awaitable);
            
            // 生产消息
            queue->push("msg_" + std::to_string(i));
        }
        
        producer_done = true;
    }, asio::detached);
    
    // 消费者
    asio::co_spawn(shared_strand, [&]() -> asio::awaitable<void> {
        // 等待一下让生产者先运行
        co_await asio::steady_timer(shared_strand, 100ms).async_wait(asio::use_awaitable);
        
        // 消费所有消息
        for (int i = 0; i < 5; ++i) {
            auto msg = co_await queue->async_read_msg(asio::use_awaitable);
            
            auto guard = co_await mutex->async_lock(asio::use_awaitable);
            received_messages.push_back(msg);
        }
        
        consumer_done = true;
    }, asio::detached);
    
    io_context.run();
    
    EXPECT_TRUE(producer_done);
    EXPECT_TRUE(consumer_done);
    EXPECT_EQ(received_messages.size(), 5);
    
    // 验证消息顺序
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(received_messages[i], "msg_" + std::to_string(i));
    }
}

// ============================================================================
// 测试 5: 多个协程并发访问共享组件
// ============================================================================

TEST(SharedStrandSimpleTest, MultipleConcurrentCoroutines) {
    asio::io_context io_context;
    asio::strand<asio::any_io_executor> shared_strand = asio::make_strand(io_context.get_executor());
    
    auto mutex = std::make_shared<async_mutex>(shared_strand);
    std::atomic<int> counter{0};
    
    const int num_coroutines = 10;
    const int increments_per_coroutine = 100;
    
    std::atomic<int> completed_coroutines{0};
    
    // 启动多个协程，每个都锁定 mutex 并增加计数器
    for (int i = 0; i < num_coroutines; ++i) {
        asio::co_spawn(shared_strand, [&]() -> asio::awaitable<void> {
            for (int j = 0; j < increments_per_coroutine; ++j) {
                auto guard = co_await mutex->async_lock(asio::use_awaitable);
                counter++;
            }
            
            completed_coroutines++;
        }, asio::detached);
    }
    
    io_context.run();
    
    EXPECT_EQ(completed_coroutines, num_coroutines);
    EXPECT_EQ(counter, num_coroutines * increments_per_coroutine);
}

// ============================================================================
// 测试 6: 验证顺序锁定（不会死锁）
// ============================================================================

TEST(SharedStrandSimpleTest, SequentialLocking) {
    asio::io_context io_context;
    asio::strand<asio::any_io_executor> shared_strand = asio::make_strand(io_context.get_executor());
    
    auto mutex1 = std::make_shared<async_mutex>(shared_strand);
    auto mutex2 = std::make_shared<async_mutex>(shared_strand);
    
    std::atomic<int> completed{0};
    
    // 任务 1: mutex1 -> mutex2
    asio::co_spawn(shared_strand, [&]() -> asio::awaitable<void> {
        auto g1 = co_await mutex1->async_lock(asio::use_awaitable);
        auto g2 = co_await mutex2->async_lock(asio::use_awaitable);
        completed++;
    }, asio::detached);
    
    // 任务 2: mutex1 -> mutex2（相同顺序，安全）
    asio::co_spawn(shared_strand, [&]() -> asio::awaitable<void> {
        auto g1 = co_await mutex1->async_lock(asio::use_awaitable);
        auto g2 = co_await mutex2->async_lock(asio::use_awaitable);
        completed++;
    }, asio::detached);
    
    io_context.run();
    
    EXPECT_EQ(completed, 2);
}

// ============================================================================
// 主函数
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

