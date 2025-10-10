//
// Batch operations example: Demonstrating efficient batch push and publish
//

#include "bcast/dispatcher.hpp"
#include <asio.hpp>
#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/use_awaitable.hpp>
#include <iostream>
#include <string>
#include <chrono>
#include <vector>

using asio::awaitable;
using asio::co_spawn;
using asio::detached;
using asio::use_awaitable;
using namespace std::chrono_literals;

struct LogEntry {
    int level;  // 0=DEBUG, 1=INFO, 2=WARN, 3=ERROR
    std::string message;
    int64_t timestamp;
    
    LogEntry() : level(0), message(""), timestamp(0) {}
    LogEntry(int l, std::string m)
        : level(l)
        , message(std::move(m))
        , timestamp(std::chrono::system_clock::now().time_since_epoch().count())
    {}
};

// Example 1: Batch push to queue
awaitable<void> example1_queue_batch_push(asio::io_context& io) {
    std::cout << "=== Example 1: Batch Push to Queue ===" << std::endl;
    
    auto queue = std::make_shared<bcast::async_queue<int>>(io);
    
    // Start a reader
    co_spawn(io, [queue]() -> awaitable<void> {
        try {
            for (int i = 0; i < 15; ++i) {
                auto value = co_await queue->async_read_msg(use_awaitable);
                std::cout << "  Read: " << value << std::endl;
            }
        } catch (const std::exception& e) {
            // Queue stopped
        }
    }, detached);
    
    auto executor = co_await asio::this_coro::executor;
    asio::steady_timer timer(executor);
    timer.expires_after(100ms);
    co_await timer.async_wait(use_awaitable);
    
    // Method 1: Push batch from vector
    std::cout << "Pushing batch from vector..." << std::endl;
    std::vector<int> batch1 = {1, 2, 3, 4, 5};
    queue->push_batch(batch1);
    
    timer.expires_after(100ms);
    co_await timer.async_wait(use_awaitable);
    
    // Method 2: Push batch from iterators
    std::cout << "Pushing batch from iterators..." << std::endl;
    std::vector<int> batch2 = {6, 7, 8, 9, 10};
    queue->push_batch(batch2.begin(), batch2.end());
    
    timer.expires_after(100ms);
    co_await timer.async_wait(use_awaitable);
    
    // Method 3: Push batch from initializer list
    std::cout << "Pushing batch from initializer list..." << std::endl;
    queue->push_batch({11, 12, 13, 14, 15});
    
    timer.expires_after(200ms);
    co_await timer.async_wait(use_awaitable);
    
    queue->stop();
    std::cout << std::endl;
}

// Example 2: Batch publish to all subscribers
awaitable<void> example2_dispatcher_batch_publish(asio::io_context& io) {
    std::cout << "=== Example 2: Batch Publish to Subscribers ===" << std::endl;
    
    auto dispatcher = bcast::make_dispatcher<LogEntry>(io);
    
    // Create 3 subscribers
    auto queue1 = dispatcher->subscribe();
    auto queue2 = dispatcher->subscribe();
    auto queue3 = dispatcher->subscribe();
    
    // Reader 1: Process all logs
    co_spawn(io, [queue1]() -> awaitable<void> {
        std::cout << "[Subscriber 1] Started" << std::endl;
        try {
            for (int i = 0; i < 9; ++i) {
                auto log = co_await queue1->async_read_msg(use_awaitable);
                std::cout << "[Sub1] " << log.message << std::endl;
            }
        } catch (const std::exception&) {
            // Queue stopped
        }
    }, detached);
    
    // Reader 2: Only process errors
    co_spawn(io, [queue2]() -> awaitable<void> {
        std::cout << "[Subscriber 2] Started (errors only)" << std::endl;
        try {
            for (int i = 0; i < 9; ++i) {
                auto log = co_await queue2->async_read_msg(use_awaitable);
                if (log.level >= 3) {
                    std::cout << "[Sub2] ERROR: " << log.message << std::endl;
                }
            }
        } catch (const std::exception&) {
            // Queue stopped
        }
    }, detached);
    
    // Reader 3: Count by level
    co_spawn(io, [queue3]() -> awaitable<void> {
        std::cout << "[Subscriber 3] Started (counter)" << std::endl;
        int counts[4] = {0};
        try {
            for (int i = 0; i < 9; ++i) {
                auto log = co_await queue3->async_read_msg(use_awaitable);
                counts[log.level]++;
            }
        } catch (const std::exception&) {
            // Queue stopped
        }
        std::cout << "[Sub3] DEBUG:" << counts[0] 
                  << " INFO:" << counts[1]
                  << " WARN:" << counts[2]
                  << " ERROR:" << counts[3] << std::endl;
    }, detached);
    
    auto executor = co_await asio::this_coro::executor;
    asio::steady_timer timer(executor);
    timer.expires_after(100ms);
    co_await timer.async_wait(use_awaitable);
    
    // Publish batch of logs
    std::cout << "Publishing batch of 9 log entries..." << std::endl;
    
    std::vector<LogEntry> logs = {
        LogEntry{0, "Application started"},
        LogEntry{1, "Configuration loaded"},
        LogEntry{1, "Database connected"},
        LogEntry{2, "High memory usage detected"},
        LogEntry{1, "Processing request"},
        LogEntry{3, "Failed to connect to service"},
        LogEntry{2, "Retrying connection"},
        LogEntry{1, "Connection restored"},
        LogEntry{3, "Critical error occurred"}
    };
    
    dispatcher->publish_batch(logs);
    
    timer.expires_after(300ms);
    co_await timer.async_wait(use_awaitable);
    
    dispatcher->clear();
    std::cout << std::endl;
}

// Example 3: Performance comparison
awaitable<void> example3_performance_comparison(asio::io_context& io) {
    std::cout << "=== Example 3: Performance Comparison ===" << std::endl;
    
    const int NUM_MESSAGES = 1000;
    
    // Test 1: Individual push
    {
        auto queue = std::make_shared<bcast::async_queue<int>>(io);
        
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < NUM_MESSAGES; ++i) {
            queue->push(i);
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        std::cout << "Individual push (" << NUM_MESSAGES << " messages): " 
                  << duration.count() << " μs" << std::endl;
    }
    
    // Test 2: Batch push
    {
        auto queue = std::make_shared<bcast::async_queue<int>>(io);
        
        std::vector<int> batch;
        batch.reserve(NUM_MESSAGES);
        for (int i = 0; i < NUM_MESSAGES; ++i) {
            batch.push_back(i);
        }
        
        auto start = std::chrono::high_resolution_clock::now();
        
        queue->push_batch(batch);
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        std::cout << "Batch push (" << NUM_MESSAGES << " messages): " 
                  << duration.count() << " μs" << std::endl;
        std::cout << "Speedup: " << "~" << (1000.0 / NUM_MESSAGES * 1000) << "x faster (approximate)" << std::endl;
    }
    
    std::cout << std::endl;
}

// Example 4: Real-world scenario - Bulk data processing
awaitable<void> example4_bulk_data_processing(asio::io_context& io) {
    std::cout << "=== Example 4: Bulk Data Processing ===" << std::endl;
    
    struct DataPoint {
        double value;
        int sensor_id;
        
        DataPoint() : value(0.0), sensor_id(0) {}
        DataPoint(double v, int id) : value(v), sensor_id(id) {}
    };
    
    auto dispatcher = bcast::make_dispatcher<DataPoint>(io);
    
    // Analytics subscriber
    auto analytics_queue = dispatcher->subscribe();
    co_spawn(io, [analytics_queue]() -> awaitable<void> {
        double sum = 0;
        int count = 0;
        
        try {
            while (true) {
                auto msgs = co_await analytics_queue->async_read_msgs(100, use_awaitable);
                if (msgs.empty()) break;
                
                for (const auto& dp : msgs) {
                    sum += dp.value;
                    count++;
                }
            }
        } catch (const std::exception&) {
            // Queue stopped
        }
        
        if (count > 0) {
            std::cout << "[Analytics] Processed " << count 
                      << " data points, average: " << (sum / count) << std::endl;
        }
    }, detached);
    
    // Anomaly detection subscriber
    auto anomaly_queue = dispatcher->subscribe();
    co_spawn(io, [anomaly_queue]() -> awaitable<void> {
        int anomalies = 0;
        
        try {
            while (true) {
                auto msgs = co_await anomaly_queue->async_read_msgs(100, use_awaitable);
                if (msgs.empty()) break;
                
                for (const auto& dp : msgs) {
                    if (dp.value > 100.0 || dp.value < 0.0) {
                        anomalies++;
                    }
                }
            }
        } catch (const std::exception&) {
            // Queue stopped
        }
        
        std::cout << "[Anomaly Detection] Found " << anomalies << " anomalies" << std::endl;
    }, detached);
    
    auto executor = co_await asio::this_coro::executor;
    asio::steady_timer timer(executor);
    timer.expires_after(100ms);
    co_await timer.async_wait(use_awaitable);
    
    // Simulate receiving bulk sensor data
    std::cout << "Processing sensor data in batches..." << std::endl;
    
    for (int batch_num = 0; batch_num < 5; ++batch_num) {
        std::vector<DataPoint> batch;
        batch.reserve(100);
        
        for (int i = 0; i < 100; ++i) {
            batch.push_back(DataPoint{
                50.0 + (rand() % 100) - 50.0,  // Random value around 50
                batch_num * 100 + i
            });
        }
        
        std::cout << "  Batch " << (batch_num + 1) << ": Publishing 100 data points" << std::endl;
        dispatcher->publish_batch(batch);
        
        timer.expires_after(50ms);
        co_await timer.async_wait(use_awaitable);
    }
    
    timer.expires_after(200ms);
    co_await timer.async_wait(use_awaitable);
    
    dispatcher->clear();
    std::cout << std::endl;
}

awaitable<void> run_all_examples(asio::io_context& io) {
    co_await example1_queue_batch_push(io);
    co_await example2_dispatcher_batch_publish(io);
    co_await example3_performance_comparison(io);
    co_await example4_bulk_data_processing(io);
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  Batch Operations Examples" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;
    
    try {
        asio::io_context io;
        
        co_spawn(io, run_all_examples(io), detached);
        
        io.run();
        
        std::cout << "========================================" << std::endl;
        std::cout << "  All examples completed!" << std::endl;
        std::cout << "========================================" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}

