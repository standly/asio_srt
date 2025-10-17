//
// Performance benchmark for async publish-subscribe pattern
//

#include "acore/dispatcher.hpp"
#include <asio.hpp>
#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/use_awaitable.hpp>
#include <iostream>
#include <chrono>
#include <thread>
#include <atomic>
#include <iomanip>

using namespace std::chrono;

// Benchmark configuration
struct BenchmarkConfig {
    size_t num_subscribers = 10;
    size_t num_messages = 10000;
    size_t num_threads = 4;
    bool verbose = false;
};

// Simple message type
struct BenchmarkMessage {
    uint64_t id;
    uint64_t timestamp_ns;
    std::array<uint8_t, 64> payload;  // 64 bytes payload
};

// Benchmark results
struct BenchmarkResults {
    double throughput_msg_per_sec = 0.0;
    double latency_avg_us = 0.0;
    double latency_min_us = 0.0;
    double latency_max_us = 0.0;
    size_t messages_processed = 0;
    double duration_sec = 0.0;
};

class Benchmark {
public:
    Benchmark(const BenchmarkConfig& config)
        : config_(config)
        , messages_received_(0)
        , total_latency_us_(0)
        , min_latency_us_(std::numeric_limits<double>::max())
        , max_latency_us_(0)
    {}
    
    BenchmarkResults run() {
        std::cout << "=== Benchmark Configuration ===" << std::endl;
        std::cout << "Subscribers:     " << config_.num_subscribers << std::endl;
        std::cout << "Messages:        " << config_.num_messages << std::endl;
        std::cout << "IO Threads:      " << config_.num_threads << std::endl;
        std::cout << "Message Size:    " << sizeof(BenchmarkMessage) << " bytes" << std::endl;
        std::cout << std::endl;
        
        asio::io_context io_context;
        auto dispatcher = acore::make_dispatcher<BenchmarkMessage>(io_context);
        
        // Create subscribers with coroutines
        std::cout << "Creating " << config_.num_subscribers << " subscribers..." << std::endl;
        
        for (size_t i = 0; i < config_.num_subscribers; ++i) {
            auto queue = dispatcher->subscribe();
            
            // Spawn a coroutine for each subscriber
            asio::co_spawn(io_context, 
                subscriber_task(queue, config_.num_messages),
                asio::detached);
        }
        
        // Start IO threads
        std::cout << "Starting " << config_.num_threads << " IO threads..." << std::endl;
        std::vector<std::thread> io_threads;
        for (size_t i = 0; i < config_.num_threads; ++i) {
            io_threads.emplace_back([&io_context]() {
                io_context.run();
            });
        }
        
        // Wait for setup
        std::this_thread::sleep_for(milliseconds(100));
        
        // Publish messages
        std::cout << "Publishing " << config_.num_messages << " messages..." << std::endl;
        
        auto start_time = high_resolution_clock::now();
        
        for (size_t i = 0; i < config_.num_messages; ++i) {
            BenchmarkMessage msg;
            msg.id = i;
            msg.timestamp_ns = duration_cast<nanoseconds>(
                high_resolution_clock::now().time_since_epoch()
            ).count();
            
            dispatcher->publish(msg);
            
            if (config_.verbose && (i % 1000 == 0)) {
                std::cout << "Published: " << i << std::endl;
            }
        }
        
        // Wait for all messages to be processed
        size_t expected_messages = config_.num_messages * config_.num_subscribers;
        std::cout << "Waiting for " << expected_messages << " messages to be processed..." << std::endl;
        
        while (messages_received_.load() < expected_messages) {
            std::this_thread::sleep_for(milliseconds(10));
        }
        
        auto end_time = high_resolution_clock::now();
        
        // Stop IO context
        io_context.stop();
        for (auto& t : io_threads) {
            t.join();
        }
        
        // Calculate results
        BenchmarkResults results;
        results.messages_processed = messages_received_.load();
        results.duration_sec = duration_cast<duration<double>>(end_time - start_time).count();
        results.throughput_msg_per_sec = results.messages_processed / results.duration_sec;
        
        if (results.messages_processed > 0) {
            results.latency_avg_us = total_latency_us_.load() / results.messages_processed;
            results.latency_min_us = min_latency_us_.load();
            results.latency_max_us = max_latency_us_.load();
        }
        
        return results;
    }
    
private:
    asio::awaitable<void> subscriber_task(
        std::shared_ptr<acore::async_queue<BenchmarkMessage>> queue,
        size_t expected_messages)
    {
        size_t received = 0;
        
        while (received < expected_messages) {
            try {
                auto msg = co_await queue->async_read_msg(asio::use_awaitable);
                
                on_message_received(msg);
                received++;
                
            } catch (const std::system_error& e) {
                // Queue stopped or error occurred
                break;
            } catch (const std::exception& e) {
                std::cerr << "Subscriber error: " << e.what() << std::endl;
                break;
            }
        }
    }
    
    void on_message_received(const BenchmarkMessage& msg) {
        // Calculate latency
        auto now_ns = duration_cast<nanoseconds>(
            high_resolution_clock::now().time_since_epoch()
        ).count();
        
        double latency_us = (now_ns - msg.timestamp_ns) / 1000.0;
        
        // Update statistics
        messages_received_.fetch_add(1, std::memory_order_relaxed);
        total_latency_us_.fetch_add(latency_us, std::memory_order_relaxed);
        
        // Update min/max (simple, not perfectly accurate in concurrent scenario)
        double current_min = min_latency_us_.load();
        while (latency_us < current_min) {
            if (min_latency_us_.compare_exchange_weak(current_min, latency_us)) {
                break;
            }
        }
        
        double current_max = max_latency_us_.load();
        while (latency_us > current_max) {
            if (max_latency_us_.compare_exchange_weak(current_max, latency_us)) {
                break;
            }
        }
    }
    
    BenchmarkConfig config_;
    std::atomic<size_t> messages_received_;
    std::atomic<double> total_latency_us_;
    std::atomic<double> min_latency_us_;
    std::atomic<double> max_latency_us_;
};

void print_results(const BenchmarkResults& results) {
    std::cout << std::endl;
    std::cout << "=== Benchmark Results ===" << std::endl;
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Duration:           " << results.duration_sec << " seconds" << std::endl;
    std::cout << "Messages Processed: " << results.messages_processed << std::endl;
    std::cout << "Throughput:         " << results.throughput_msg_per_sec << " msg/sec" << std::endl;
    std::cout << std::endl;
    std::cout << "Latency Statistics:" << std::endl;
    std::cout << "  Average:          " << results.latency_avg_us << " μs" << std::endl;
    std::cout << "  Minimum:          " << results.latency_min_us << " μs" << std::endl;
    std::cout << "  Maximum:          " << results.latency_max_us << " μs" << std::endl;
    std::cout << std::endl;
    
    // Calculate data rate
    size_t total_bytes = results.messages_processed * sizeof(BenchmarkMessage);
    double mb_per_sec = (total_bytes / (1024.0 * 1024.0)) / results.duration_sec;
    std::cout << "Data Rate:          " << mb_per_sec << " MB/sec" << std::endl;
    std::cout << "=========================" << std::endl;
}

void run_scalability_test() {
    std::cout << std::endl;
    std::cout << "=====================================" << std::endl;
    std::cout << "Running Scalability Test" << std::endl;
    std::cout << "=====================================" << std::endl;
    std::cout << std::endl;
    
    std::vector<size_t> subscriber_counts = {1, 5, 10, 20, 50, 100};
    
    for (size_t count : subscriber_counts) {
        std::cout << "--- Testing with " << count << " subscribers ---" << std::endl;
        
        BenchmarkConfig config;
        config.num_subscribers = count;
        config.num_messages = 5000;
        config.num_threads = 4;
        config.verbose = false;
        
        Benchmark benchmark(config);
        auto results = benchmark.run();
        
        std::cout << "Throughput: " << std::fixed << std::setprecision(0)
                  << results.throughput_msg_per_sec << " msg/sec, "
                  << "Avg Latency: " << std::setprecision(2) 
                  << results.latency_avg_us << " μs" << std::endl;
        std::cout << std::endl;
    }
}

void run_thread_scalability_test() {
    std::cout << std::endl;
    std::cout << "=====================================" << std::endl;
    std::cout << "Running Thread Scalability Test" << std::endl;
    std::cout << "=====================================" << std::endl;
    std::cout << std::endl;
    
    std::vector<size_t> thread_counts = {1, 2, 4, 8};
    
    for (size_t count : thread_counts) {
        std::cout << "--- Testing with " << count << " IO threads ---" << std::endl;
        
        BenchmarkConfig config;
        config.num_subscribers = 10;
        config.num_messages = 5000;
        config.num_threads = count;
        config.verbose = false;
        
        Benchmark benchmark(config);
        auto results = benchmark.run();
        
        std::cout << "Throughput: " << std::fixed << std::setprecision(0)
                  << results.throughput_msg_per_sec << " msg/sec, "
                  << "Avg Latency: " << std::setprecision(2) 
                  << results.latency_avg_us << " μs" << std::endl;
        std::cout << std::endl;
    }
}

int main() {
    try {
        std::cout << "Async Publish-Subscribe Pattern Benchmark" << std::endl;
        std::cout << "==========================================" << std::endl;
        std::cout << std::endl;
        
        // Basic benchmark
        {
            BenchmarkConfig config;
            config.num_subscribers = 10;
            config.num_messages = 10000;
            config.num_threads = 4;
            config.verbose = false;
            
            Benchmark benchmark(config);
            auto results = benchmark.run();
            print_results(results);
        }
        
        // Scalability tests
        run_scalability_test();
        run_thread_scalability_test();
        
        std::cout << std::endl;
        std::cout << "All benchmarks completed successfully!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Benchmark failed: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
