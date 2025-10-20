//
// 共享 Strand 单元测试
//
// 测试所有 ACORE 组件在共享 strand 情况下的正确性
//

#include "acore/async_mutex.hpp"
#include "acore/async_queue.hpp"
#include "acore/async_barrier.hpp"
#include "acore/async_latch.hpp"
#include "acore/async_periodic_timer.hpp"
#include "acore/async_rate_limiter.hpp"
#include "acore/async_auto_reset_event.hpp"
#include "acore/async_event.hpp"
#include "acore/async_waitgroup.hpp"
#include "acore/async_semaphore.hpp"
#include "acore/dispatcher.hpp"

#include <asio.hpp>
#include <gtest/gtest.h>
#include <chrono>
#include <vector>
#include <atomic>

using namespace acore;
using namespace std::chrono_literals;

// ============================================================================
// 测试 1: 两个 mutex 共享 strand
// ============================================================================

TEST(SharedStrandTest, TwoMutexesSharedStrand) {
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

TEST(SharedStrandTest, MutexAndQueueSharedStrand) {
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
// 测试 3: barrier + latch 共享 strand
// ============================================================================

TEST(SharedStrandTest, BarrierAndLatchSharedStrand) {
    asio::io_context io_context;
    asio::strand<asio::any_io_executor> shared_strand = asio::make_strand(io_context.get_executor());
    
    const int num_workers = 3;
    auto barrier = std::make_shared<async_barrier>(shared_strand, num_workers);
    auto latch = std::make_shared<async_latch>(shared_strand, num_workers);
    
    std::atomic<int> phase1_count{0};
    std::atomic<int> phase2_count{0};
    
    // 启动多个 worker
    for (int i = 0; i < num_workers; ++i) {
        asio::co_spawn(shared_strand, [&, i]() -> asio::awaitable<void> {
            // Phase 1
            phase1_count++;
            co_await barrier->arrive_and_wait(asio::use_awaitable);
            
            // Phase 2 (所有人都到达 barrier 后才能执行)
            phase2_count++;
            latch->count_down();
            
            co_return;
        }, asio::detached);
    }
    
    // 等待所有 worker 完成
    asio::co_spawn(shared_strand, [&]() -> asio::awaitable<void> {
        co_await latch->async_wait(asio::use_awaitable);
        
        // 验证
        EXPECT_EQ(phase1_count, num_workers);
        EXPECT_EQ(phase2_count, num_workers);
    }, asio::detached);
    
    io_context.run();
}

// ============================================================================
// 测试 4: timer + rate_limiter 共享 strand
// ============================================================================

TEST(SharedStrandTest, TimerAndRateLimiterSharedStrand) {
    asio::io_context io_context;
    asio::strand<asio::any_io_executor> shared_strand = asio::make_strand(io_context.get_executor());
    
    auto timer = std::make_shared<async_periodic_timer>(shared_strand, 100ms);
    auto limiter = std::make_shared<async_rate_limiter>(shared_strand, 10, 1s);
    
    std::atomic<int> timer_ticks{0};
    std::atomic<int> limiter_acquires{0};
    std::atomic<bool> done{false};
    
    // 定时器任务
    asio::co_spawn(shared_strand, [&]() -> asio::awaitable<void> {
        timer->start();
        
        for (int i = 0; i < 3; ++i) {
            co_await timer->async_wait(asio::use_awaitable);
            timer_ticks++;
        }
        
        timer->stop();
    }, asio::detached);
    
    // 限流器任务
    asio::co_spawn(shared_strand, [&]() -> asio::awaitable<void> {
        for (int i = 0; i < 3; ++i) {
            co_await limiter->async_acquire(asio::use_awaitable);
            limiter_acquires++;
        }
        
        done = true;
    }, asio::detached);
    
    io_context.run();
    
    EXPECT_TRUE(done);
    EXPECT_EQ(timer_ticks, 3);
    EXPECT_EQ(limiter_acquires, 3);
}

// ============================================================================
// 测试 5: event + waitgroup 共享 strand
// ============================================================================

TEST(SharedStrandTest, EventAndWaitgroupSharedStrand) {
    asio::io_context io_context;
    asio::strand<asio::any_io_executor> shared_strand = asio::make_strand(io_context.get_executor());
    
    auto event = std::make_shared<async_event>(shared_strand);
    auto wg = std::make_shared<async_waitgroup>(shared_strand);
    
    const int num_tasks = 3;
    std::atomic<int> completed_tasks{0};
    
    // 启动多个任务
    for (int i = 0; i < num_tasks; ++i) {
        wg->add(1);
        
        asio::co_spawn(shared_strand, [&, i]() -> asio::awaitable<void> {
            // 等待事件
            co_await event->wait(asio::use_awaitable);
            
            // 执行任务
            completed_tasks++;
            
            // 标记完成
            wg->done();
        }, asio::detached);
    }
    
    // 触发事件
    asio::co_spawn(shared_strand, [&]() -> asio::awaitable<void> {
        co_await asio::steady_timer(shared_strand, 50ms).async_wait(asio::use_awaitable);
        event->set();  // 触发所有等待者
    }, asio::detached);
    
    // 等待所有任务完成
    asio::co_spawn(shared_strand, [&]() -> asio::awaitable<void> {
        co_await wg->wait(asio::use_awaitable);
        EXPECT_EQ(completed_tasks, num_tasks);
    }, asio::detached);
    
    io_context.run();
}

// ============================================================================
// 测试 6: 多个组件复杂协作（模拟真实场景）
// ============================================================================

TEST(SharedStrandTest, ComplexCollaboration) {
    asio::io_context io_context;
    asio::strand<asio::any_io_executor> shared_strand = asio::make_strand(io_context.get_executor());
    
    // 创建多个组件，全部共享 strand
    auto mutex = std::make_shared<async_mutex>(shared_strand);
    auto queue = std::make_shared<async_queue<std::string>>(io_context, shared_strand);
    auto barrier = std::make_shared<async_barrier>(shared_strand, 2);
    auto limiter = std::make_shared<async_rate_limiter>(shared_strand, 10, 1s);
    
    std::vector<std::string> received_messages;
    std::atomic<bool> producer_done{false};
    std::atomic<bool> consumer_done{false};
    
    // 生产者
    asio::co_spawn(shared_strand, [&]() -> asio::awaitable<void> {
        for (int i = 0; i < 5; ++i) {
            // 限流
            co_await limiter->async_acquire(asio::use_awaitable);
            
            // 锁定
            auto guard = co_await mutex->async_lock(asio::use_awaitable);
            
            // 生产消息
            queue->push("msg_" + std::to_string(i));
        }
        
        // 等待消费者准备好
        co_await barrier->arrive_and_wait(asio::use_awaitable);
        producer_done = true;
    }, asio::detached);
    
    // 消费者
    asio::co_spawn(shared_strand, [&]() -> asio::awaitable<void> {
        // 等待生产者
        co_await barrier->arrive_and_wait(asio::use_awaitable);
        
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
// 测试 7: semaphore + auto_reset_event 共享 strand
// ============================================================================

TEST(SharedStrandTest, SemaphoreAndAutoResetEventSharedStrand) {
    asio::io_context io_context;
    asio::strand<asio::any_io_executor> shared_strand = asio::make_strand(io_context.get_executor());
    
    auto semaphore = std::make_shared<async_semaphore>(shared_strand, 2);
    auto event = std::make_shared<async_auto_reset_event>(shared_strand);
    
    std::atomic<int> acquired_count{0};
    std::atomic<int> event_signaled_count{0};
    
    // 任务 1: 使用 semaphore
    asio::co_spawn(shared_strand, [&]() -> asio::awaitable<void> {
        co_await semaphore->acquire(asio::use_awaitable);
        acquired_count++;
        
        // 触发事件
        event->set();
        
        semaphore->release();
    }, asio::detached);
    
    // 任务 2: 等待事件
    asio::co_spawn(shared_strand, [&]() -> asio::awaitable<void> {
        co_await event->wait(asio::use_awaitable);
        event_signaled_count++;
        
        co_await semaphore->acquire(asio::use_awaitable);
        acquired_count++;
        semaphore->release();
    }, asio::detached);
    
    io_context.run();
    
    EXPECT_EQ(acquired_count, 2);
    EXPECT_EQ(event_signaled_count, 1);
}

// ============================================================================
// 测试 8: dispatcher 共享 strand
// ============================================================================

TEST(SharedStrandTest, DispatcherSharedStrand) {
    asio::io_context io_context;
    asio::strand<asio::any_io_executor> shared_strand = asio::make_strand(io_context.get_executor());
    
    // 注意：dispatcher 目前不支持外部 strand，使用默认构造函数
    auto dispatcher = std::make_shared<acore::dispatcher<int>>(io_context);
    
    // 订阅者 1
    auto queue1 = dispatcher->subscribe();
    
    // 订阅者 2  
    auto queue2 = dispatcher->subscribe();
    
    std::atomic<int> received1{0};
    std::atomic<int> received2{0};
    std::atomic<bool> done{false};
    
    // 发布消息
    asio::co_spawn(shared_strand, [&]() -> asio::awaitable<void> {
        dispatcher->publish(42);
        dispatcher->publish(100);
        co_return;
    }, asio::detached);
    
    // 订阅者 1 接收
    asio::co_spawn(shared_strand, [&]() -> asio::awaitable<void> {
        auto msg1 = co_await queue1->async_read_msg(asio::use_awaitable);
        received1 += msg1;
        
        auto msg2 = co_await queue1->async_read_msg(asio::use_awaitable);
        received1 += msg2;
    }, asio::detached);
    
    // 订阅者 2 接收
    asio::co_spawn(shared_strand, [&]() -> asio::awaitable<void> {
        auto msg1 = co_await queue2->async_read_msg(asio::use_awaitable);
        received2 += msg1;
        
        auto msg2 = co_await queue2->async_read_msg(asio::use_awaitable);
        received2 += msg2;
        
        done = true;
    }, asio::detached);
    
    io_context.run();
    
    EXPECT_TRUE(done);
    EXPECT_EQ(received1, 142);  // 42 + 100
    EXPECT_EQ(received2, 142);  // 42 + 100
}

// ============================================================================
// 测试 9: 多个协程并发访问共享组件
// ============================================================================

TEST(SharedStrandTest, MultipleConcurrentCoroutines) {
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
// 测试 10: 性能对比 - 共享 vs 独立 strand
// ============================================================================

TEST(SharedStrandTest, PerformanceComparison) {
    using clock_type = std::chrono::steady_clock;
    
    const int num_operations = 1000;
    
    // 测试 1: 共享 strand
    {
        asio::io_context io_context;
        asio::strand<asio::any_io_executor> shared_strand = asio::make_strand(io_context.get_executor());
        
        auto mutex1 = std::make_shared<async_mutex>(shared_strand);
        auto mutex2 = std::make_shared<async_mutex>(shared_strand);
        
        std::atomic<bool> done{false};
        auto start = clock_type::now();
        
        asio::co_spawn(shared_strand, [&]() -> asio::awaitable<void> {
            for (int i = 0; i < num_operations; ++i) {
                auto guard1 = co_await mutex1->async_lock(asio::use_awaitable);
                auto guard2 = co_await mutex2->async_lock(asio::use_awaitable);
            }
            done = true;
        }, asio::detached);
        
        io_context.run();
        auto elapsed_shared = clock_type::now() - start;
        
        EXPECT_TRUE(done);
        
        auto ms_shared = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed_shared).count();
        std::cout << "  共享 Strand: " << num_operations << " 次操作耗时 " << ms_shared << "ms" << std::endl;
    }
    
    // 测试 2: 独立 strand
    {
        asio::io_context io_context;
        
        auto mutex1 = std::make_shared<async_mutex>(io_context.get_executor());
        auto mutex2 = std::make_shared<async_mutex>(io_context.get_executor());
        
        std::atomic<bool> done{false};
        auto start = clock_type::now();
        
        asio::co_spawn(io_context, [&]() -> asio::awaitable<void> {
            for (int i = 0; i < num_operations; ++i) {
                auto guard1 = co_await mutex1->async_lock(asio::use_awaitable);
                auto guard2 = co_await mutex2->async_lock(asio::use_awaitable);
            }
            done = true;
        }, asio::detached);
        
        io_context.run();
        auto elapsed_independent = clock_type::now() - start;
        
        EXPECT_TRUE(done);
        
        auto ms_independent = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed_independent).count();
        std::cout << "  独立 Strand: " << num_operations << " 次操作耗时 " << ms_independent << "ms" << std::endl;
    }
}

// ============================================================================
// 测试 11: 验证没有死锁
// ============================================================================

TEST(SharedStrandTest, NoDeadlock) {
    asio::io_context io_context;
    asio::strand<asio::any_io_executor> shared_strand = asio::make_strand(io_context.get_executor());
    
    auto mutex1 = std::make_shared<async_mutex>(shared_strand);
    auto mutex2 = std::make_shared<async_mutex>(shared_strand);
    auto mutex3 = std::make_shared<async_mutex>(shared_strand);
    
    std::atomic<int> completed{0};
    
    // 任务 1: mutex1 -> mutex2 -> mutex3
    asio::co_spawn(shared_strand, [&]() -> asio::awaitable<void> {
        auto g1 = co_await mutex1->async_lock(asio::use_awaitable);
        auto g2 = co_await mutex2->async_lock(asio::use_awaitable);
        auto g3 = co_await mutex3->async_lock(asio::use_awaitable);
        completed++;
    }, asio::detached);
    
    // 任务 2: mutex3 -> mutex1 -> mutex2
    asio::co_spawn(shared_strand, [&]() -> asio::awaitable<void> {
        auto g3 = co_await mutex3->async_lock(asio::use_awaitable);
        auto g1 = co_await mutex1->async_lock(asio::use_awaitable);
        auto g2 = co_await mutex2->async_lock(asio::use_awaitable);
        completed++;
    }, asio::detached);
    
    // 任务 3: mutex2 -> mutex3 -> mutex1
    asio::co_spawn(shared_strand, [&]() -> asio::awaitable<void> {
        auto g2 = co_await mutex2->async_lock(asio::use_awaitable);
        auto g3 = co_await mutex3->async_lock(asio::use_awaitable);
        auto g1 = co_await mutex1->async_lock(asio::use_awaitable);
        completed++;
    }, asio::detached);
    
    // 设置超时保护
    asio::steady_timer timeout_timer(io_context, 5s);
    timeout_timer.async_wait([&](auto ec) {
        if (!ec) {
            // 超时，强制停止
            io_context.stop();
            FAIL() << "测试超时！可能发生死锁";
        }
    });
    
    io_context.run();
    timeout_timer.cancel();
    
    EXPECT_EQ(completed, 3);
}

// ============================================================================
// 主函数
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

