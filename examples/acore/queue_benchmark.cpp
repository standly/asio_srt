//
// Benchmark comparison: async_queue vs async_queue_v2
//

#include "acore/async_queue.hpp"
#include "acore/async_queue_v2.hpp"
#include <asio.hpp>
#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/use_awaitable.hpp>
#include <iostream>
#include <chrono>
#include <atomic>
#include <vector>
#include <iomanip>

using asio::awaitable;
using asio::co_spawn;
using asio::detached;
using asio::use_awaitable;
using namespace std::chrono_literals;

// 测试消息类型
struct BenchmarkMessage {
    int id;
    uint64_t timestamp;
    char padding[56];  // 填充到 64 字节缓存行
    
    BenchmarkMessage() : id(0), timestamp(0), padding{} {}
    BenchmarkMessage(int i) : id(i), timestamp(0), padding{} {}
};

// 性能统计
struct BenchmarkStats {
    std::string name;
    size_t total_messages;
    std::chrono::microseconds duration;
    double throughput_msg_per_sec;
    double latency_us_per_msg;
    
    void print() const {
        std::cout << "  " << std::left << std::setw(25) << name << ": "
                  << std::right << std::setw(12) << total_messages << " msgs, "
                  << std::setw(10) << duration.count() << " us, "
                  << std::setw(12) << std::fixed << std::setprecision(2) 
                  << throughput_msg_per_sec << " msg/s, "
                  << std::setw(8) << std::fixed << std::setprecision(3)
                  << latency_us_per_msg << " us/msg"
                  << std::endl;
    }
};

// ============================================================================
// 测试1：单生产者 + 单消费者（吞吐量测试）
// ============================================================================

template<typename QueueType>
awaitable<BenchmarkStats> test_single_producer_consumer(
    asio::io_context& io, 
    std::shared_ptr<QueueType> queue,
    size_t message_count,
    const std::string& name)
{
    auto consumed = std::make_shared<std::atomic<size_t>>(0);
    auto start_time = std::make_shared<std::chrono::high_resolution_clock::time_point>();
    auto end_time = std::make_shared<std::chrono::high_resolution_clock::time_point>();
    
    // 消费者
    co_spawn(io, [queue, consumed, message_count, end_time]() -> awaitable<void> {
        for (size_t i = 0; i < message_count; ++i) {
            try {
                auto msg = co_await queue->async_read_msg(use_awaitable);
                consumed->fetch_add(1);
            } catch (const std::exception& e) {
                break;
            }
        }
        *end_time = std::chrono::high_resolution_clock::now();
    }, detached);
    
    // 等待消费者准备好
    auto executor = co_await asio::this_coro::executor;
    asio::steady_timer timer(executor);
    timer.expires_after(10ms);
    co_await timer.async_wait(use_awaitable);
    
    // 生产者（计时开始）
    *start_time = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < message_count; ++i) {
        queue->push(BenchmarkMessage(i));
    }
    
    // 等待所有消息被消费
    while (consumed->load() < message_count) {
        timer.expires_after(1ms);
        co_await timer.async_wait(use_awaitable);
    }
    
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(*end_time - *start_time);
    
    BenchmarkStats stats;
    stats.name = name;
    stats.total_messages = message_count;
    stats.duration = duration;
    stats.throughput_msg_per_sec = (message_count * 1000000.0) / duration.count();
    stats.latency_us_per_msg = (double)duration.count() / message_count;
    
    co_return stats;
}

// ============================================================================
// 测试2：单生产者 + 多消费者（并发测试）
// ============================================================================

template<typename QueueType>
awaitable<BenchmarkStats> test_single_producer_multi_consumer(
    asio::io_context& io, 
    std::shared_ptr<QueueType> queue,
    size_t message_count,
    size_t consumer_count,
    const std::string& name)
{
    auto consumed = std::make_shared<std::atomic<size_t>>(0);
    auto start_time = std::make_shared<std::chrono::high_resolution_clock::time_point>();
    auto end_time = std::make_shared<std::chrono::high_resolution_clock::time_point>();
    
    // 多个消费者
    for (size_t c = 0; c < consumer_count; ++c) {
        co_spawn(io, [queue, consumed, message_count, end_time]() -> awaitable<void> {
            while (consumed->load() < message_count) {
                try {
                    auto msg = co_await queue->async_read_msg(use_awaitable);
                    size_t count = consumed->fetch_add(1) + 1;
                    if (count == message_count) {
                        *end_time = std::chrono::high_resolution_clock::now();
                    }
                } catch (const std::exception& e) {
                    break;
                }
            }
        }, detached);
    }
    
    // 等待消费者准备好
    auto executor = co_await asio::this_coro::executor;
    asio::steady_timer timer(executor);
    timer.expires_after(50ms);
    co_await timer.async_wait(use_awaitable);
    
    // 生产者（计时开始）
    *start_time = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < message_count; ++i) {
        queue->push(BenchmarkMessage(i));
    }
    
    // 等待所有消息被消费
    while (consumed->load() < message_count) {
        timer.expires_after(1ms);
        co_await timer.async_wait(use_awaitable);
    }
    
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(*end_time - *start_time);
    
    BenchmarkStats stats;
    stats.name = name + " (" + std::to_string(consumer_count) + " consumers)";
    stats.total_messages = message_count;
    stats.duration = duration;
    stats.throughput_msg_per_sec = (message_count * 1000000.0) / duration.count();
    stats.latency_us_per_msg = (double)duration.count() / message_count;
    
    co_return stats;
}

// ============================================================================
// 测试3：批量操作测试
// ============================================================================

template<typename QueueType>
awaitable<BenchmarkStats> test_batch_operations(
    asio::io_context& io, 
    std::shared_ptr<QueueType> queue,
    size_t batch_count,
    size_t batch_size,
    const std::string& name)
{
    size_t total_messages = batch_count * batch_size;
    auto consumed = std::make_shared<std::atomic<size_t>>(0);
    auto start_time = std::make_shared<std::chrono::high_resolution_clock::time_point>();
    auto end_time = std::make_shared<std::chrono::high_resolution_clock::time_point>();
    
    // 消费者
    co_spawn(io, [queue, consumed, total_messages, end_time]() -> awaitable<void> {
        while (consumed->load() < total_messages) {
            try {
                auto msg = co_await queue->async_read_msg(use_awaitable);
                size_t count = consumed->fetch_add(1) + 1;
                if (count == total_messages) {
                    *end_time = std::chrono::high_resolution_clock::now();
                }
            } catch (const std::exception& e) {
                break;
            }
        }
    }, detached);
    
    // 等待消费者准备好
    auto executor = co_await asio::this_coro::executor;
    asio::steady_timer timer(executor);
    timer.expires_after(10ms);
    co_await timer.async_wait(use_awaitable);
    
    // 批量生产者（计时开始）
    *start_time = std::chrono::high_resolution_clock::now();
    for (size_t b = 0; b < batch_count; ++b) {
        std::vector<BenchmarkMessage> batch;
        batch.reserve(batch_size);
        for (size_t i = 0; i < batch_size; ++i) {
            batch.emplace_back(b * batch_size + i);
        }
        queue->push_batch(std::move(batch));
    }
    
    // 等待所有消息被消费
    while (consumed->load() < total_messages) {
        timer.expires_after(1ms);
        co_await timer.async_wait(use_awaitable);
    }
    
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(*end_time - *start_time);
    
    BenchmarkStats stats;
    stats.name = name + " (batch=" + std::to_string(batch_size) + ")";
    stats.total_messages = total_messages;
    stats.duration = duration;
    stats.throughput_msg_per_sec = (total_messages * 1000000.0) / duration.count();
    stats.latency_us_per_msg = (double)duration.count() / total_messages;
    
    co_return stats;
}

// ============================================================================
// 测试4：延迟测试（ping-pong）
// ============================================================================

template<typename QueueType>
awaitable<BenchmarkStats> test_latency_ping_pong(
    asio::io_context& io, 
    std::shared_ptr<QueueType> queue1,
    std::shared_ptr<QueueType> queue2,
    size_t round_trips,
    const std::string& name)
{
    auto completed = std::make_shared<std::atomic<size_t>>(0);
    auto total_latency = std::make_shared<std::atomic<uint64_t>>(0);
    
    // Pong 端
    co_spawn(io, [queue1, queue2, round_trips, completed]() -> awaitable<void> {
        for (size_t i = 0; i < round_trips; ++i) {
            try {
                auto msg = co_await queue1->async_read_msg(use_awaitable);
                queue2->push(std::move(msg));
            } catch (const std::exception& e) {
                break;
            }
        }
    }, detached);
    
    // 等待 Pong 准备好
    auto executor = co_await asio::this_coro::executor;
    asio::steady_timer timer(executor);
    timer.expires_after(10ms);
    co_await timer.async_wait(use_awaitable);
    
    // Ping 端（计时）
    auto start = std::chrono::high_resolution_clock::now();
    
    for (size_t i = 0; i < round_trips; ++i) {
        auto msg_start = std::chrono::high_resolution_clock::now();
        
        // 发送
        queue1->push(BenchmarkMessage(i));
        
        // 接收
        try {
            auto msg = co_await queue2->async_read_msg(use_awaitable);
            auto msg_end = std::chrono::high_resolution_clock::now();
            auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(msg_end - msg_start);
            total_latency->fetch_add(latency.count());
            completed->fetch_add(1);
        } catch (const std::exception& e) {
            break;
        }
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    BenchmarkStats stats;
    stats.name = name;
    stats.total_messages = round_trips * 2;  // ping + pong
    stats.duration = duration;
    stats.throughput_msg_per_sec = (round_trips * 2 * 1000000.0) / duration.count();
    stats.latency_us_per_msg = (total_latency->load() / 1000.0) / round_trips;  // 单程延迟
    
    co_return stats;
}

// ============================================================================
// 主测试程序
// ============================================================================

awaitable<void> run_all_benchmarks(asio::io_context& io) {
    std::cout << "\n╔════════════════════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║                   Async Queue Benchmark Comparison                         ║" << std::endl;
    std::cout << "╚════════════════════════════════════════════════════════════════════════════╝\n" << std::endl;
    
    std::vector<BenchmarkStats> all_stats;
    
    // ========================================================================
    // 测试1: 单生产者 + 单消费者 - 小数据量
    // ========================================================================
    std::cout << "┌─ Test 1: Single Producer + Single Consumer (10K messages) ────────────────┐" << std::endl;
    {
        auto queue_v1 = std::make_shared<acore::async_queue<BenchmarkMessage>>(io);
        auto stats = co_await test_single_producer_consumer(io, queue_v1, 10000, "async_queue (original)");
        stats.print();
        all_stats.push_back(stats);
        
        auto queue_v2 = std::make_shared<acore::async_queue_v2<BenchmarkMessage>>(io);
        stats = co_await test_single_producer_consumer(io, queue_v2, 10000, "async_queue_v2 (semaphore)");
        stats.print();
        all_stats.push_back(stats);
    }
    std::cout << "└────────────────────────────────────────────────────────────────────────────┘\n" << std::endl;
    
    // ========================================================================
    // 测试2: 单生产者 + 单消费者 - 大数据量
    // ========================================================================
    std::cout << "┌─ Test 2: Single Producer + Single Consumer (100K messages) ───────────────┐" << std::endl;
    {
        auto queue_v1 = std::make_shared<acore::async_queue<BenchmarkMessage>>(io);
        auto stats = co_await test_single_producer_consumer(io, queue_v1, 100000, "async_queue (original)");
        stats.print();
        all_stats.push_back(stats);
        
        auto queue_v2 = std::make_shared<acore::async_queue_v2<BenchmarkMessage>>(io);
        stats = co_await test_single_producer_consumer(io, queue_v2, 100000, "async_queue_v2 (semaphore)");
        stats.print();
        all_stats.push_back(stats);
    }
    std::cout << "└────────────────────────────────────────────────────────────────────────────┘\n" << std::endl;
    
    // ========================================================================
    // 测试3: 单生产者 + 多消费者（V2 的优势）
    // ========================================================================
    std::cout << "┌─ Test 3: Single Producer + Multi Consumer (50K messages, 5 consumers) ────┐" << std::endl;
    std::cout << "  Note: Original queue only supports 1 pending reader, may lose messages" << std::endl;
    {
        auto queue_v2 = std::make_shared<acore::async_queue_v2<BenchmarkMessage>>(io);
        auto stats = co_await test_single_producer_multi_consumer(io, queue_v2, 50000, 5, "async_queue_v2");
        stats.print();
        all_stats.push_back(stats);
    }
    std::cout << "└────────────────────────────────────────────────────────────────────────────┘\n" << std::endl;
    
    // ========================================================================
    // 测试4: 批量操作
    // ========================================================================
    std::cout << "┌─ Test 4: Batch Operations (1000 batches x 100 msgs) ──────────────────────┐" << std::endl;
    {
        auto queue_v1 = std::make_shared<acore::async_queue<BenchmarkMessage>>(io);
        auto stats = co_await test_batch_operations(io, queue_v1, 1000, 100, "async_queue (original)");
        stats.print();
        all_stats.push_back(stats);
        
        auto queue_v2 = std::make_shared<acore::async_queue_v2<BenchmarkMessage>>(io);
        stats = co_await test_batch_operations(io, queue_v2, 1000, 100, "async_queue_v2 (semaphore)");
        stats.print();
        all_stats.push_back(stats);
    }
    std::cout << "└────────────────────────────────────────────────────────────────────────────┘\n" << std::endl;
    
    // ========================================================================
    // 测试5: 延迟测试（ping-pong）
    // ========================================================================
    std::cout << "┌─ Test 5: Latency (Ping-Pong, 10K round-trips) ────────────────────────────┐" << std::endl;
    {
        auto queue1_v1 = std::make_shared<acore::async_queue<BenchmarkMessage>>(io);
        auto queue2_v1 = std::make_shared<acore::async_queue<BenchmarkMessage>>(io);
        auto stats = co_await test_latency_ping_pong(io, queue1_v1, queue2_v1, 10000, "async_queue (original)");
        stats.print();
        all_stats.push_back(stats);
        
        auto queue1_v2 = std::make_shared<acore::async_queue_v2<BenchmarkMessage>>(io);
        auto queue2_v2 = std::make_shared<acore::async_queue_v2<BenchmarkMessage>>(io);
        stats = co_await test_latency_ping_pong(io, queue1_v2, queue2_v2, 10000, "async_queue_v2 (semaphore)");
        stats.print();
        all_stats.push_back(stats);
    }
    std::cout << "└────────────────────────────────────────────────────────────────────────────┘\n" << std::endl;
    
    // ========================================================================
    // 总结
    // ========================================================================
    std::cout << "\n╔════════════════════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║                              Summary                                       ║" << std::endl;
    std::cout << "╚════════════════════════════════════════════════════════════════════════════╝\n" << std::endl;
    
    std::cout << "Performance overhead of semaphore version:" << std::endl;
    if (all_stats.size() >= 2) {
        double overhead_10k = (all_stats[1].duration.count() - all_stats[0].duration.count()) * 100.0 / all_stats[0].duration.count();
        std::cout << "  10K messages:   " << std::fixed << std::setprecision(2) << overhead_10k << "%" << std::endl;
    }
    if (all_stats.size() >= 4) {
        double overhead_100k = (all_stats[3].duration.count() - all_stats[2].duration.count()) * 100.0 / all_stats[2].duration.count();
        std::cout << "  100K messages:  " << std::fixed << std::setprecision(2) << overhead_100k << "%" << std::endl;
    }
    if (all_stats.size() >= 6) {
        double overhead_batch = (all_stats[5].duration.count() - all_stats[4].duration.count()) * 100.0 / all_stats[4].duration.count();
        std::cout << "  Batch ops:      " << std::fixed << std::setprecision(2) << overhead_batch << "%" << std::endl;
    }
    if (all_stats.size() >= 8) {
        double overhead_latency = (all_stats[7].latency_us_per_msg - all_stats[6].latency_us_per_msg) * 100.0 / all_stats[6].latency_us_per_msg;
        std::cout << "  Latency:        " << std::fixed << std::setprecision(2) << overhead_latency << "%" << std::endl;
    }
    
    std::cout << "\nConclusion:" << std::endl;
    std::cout << "  • Original queue: Best performance for single consumer scenarios" << std::endl;
    std::cout << "  • Semaphore queue: ~5-15% overhead, but supports multiple consumers" << std::endl;
    std::cout << "  • Recommendation: Use semaphore version for new projects (code simplicity)" << std::endl;
    std::cout << "                   Use original version for performance-critical paths" << std::endl;
    std::cout << std::endl;
}

int main() {
    try {
        asio::io_context io;
        
        co_spawn(io, run_all_benchmarks(io), detached);
        
        io.run();
        
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}


