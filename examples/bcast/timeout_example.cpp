//
// Timeout example: Demonstrating read operations with timeout
//

#include "bcast/dispatcher.hpp"
#include <asio.hpp>
#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/use_awaitable.hpp>
#include <iostream>
#include <string>
#include <chrono>

using asio::awaitable;
using asio::co_spawn;
using asio::detached;
using asio::use_awaitable;
using namespace std::chrono_literals;

struct Message {
    int id;
    std::string content;
    
    Message() : id(0), content("") {}
    Message(int i, std::string c) : id(i), content(std::move(c)) {}
};

// Example 1: Simple timeout on single message read
awaitable<void> example1_simple_timeout(
    std::shared_ptr<bcast::async_queue<Message>> queue)
{
    std::cout << "=== Example 1: Simple Timeout ===" << std::endl;
    
    try {
        // Try to read a message with 2 second timeout
        std::cout << "Waiting for message with 2s timeout..." << std::endl;
        auto msg = co_await queue->async_read_msg_with_timeout(2s, use_awaitable);
        std::cout << "✅ Received: #" << msg.id << " - " << msg.content << std::endl;
    } catch (const std::system_error& e) {
        if (e.code() == std::errc::timed_out) {
            std::cout << "❌ Timeout! No message received within 2 seconds." << std::endl;
        } else {
            std::cout << "❌ Error: " << e.what() << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
    
    std::cout << std::endl;
}

// Example 2: Multiple reads with timeout
awaitable<void> example2_multiple_timeouts(
    std::shared_ptr<bcast::async_queue<Message>> queue)
{
    std::cout << "=== Example 2: Multiple Reads with Timeout ===" << std::endl;
    
    for (int i = 0; i < 5; ++i) {
        std::cout << "Read attempt " << (i + 1) << "..." << std::endl;
        try {
            auto msg = co_await queue->async_read_msg_with_timeout(1s, use_awaitable);
            std::cout << "  ✅ Message #" << msg.id << ": " << msg.content << std::endl;
        } catch (const std::system_error& e) {
            if (e.code() == std::errc::timed_out) {
                std::cout << "  ⏱️  Timeout" << std::endl;
            } else {
                std::cout << "  ❌ Error: " << e.what() << std::endl;
                break;
            }
        }
        
        // Small delay between reads
        auto executor = co_await asio::this_coro::executor;
        asio::steady_timer timer(executor);
        timer.expires_after(200ms);
        co_await timer.async_wait(use_awaitable);
    }
    
    std::cout << std::endl;
}

// Example 3: Batch read with timeout
awaitable<void> example3_batch_timeout(
    std::shared_ptr<bcast::async_queue<Message>> queue)
{
    std::cout << "=== Example 3: Batch Read with Timeout ===" << std::endl;
    
    // Try to read up to 10 messages, but timeout after 3 seconds
    std::cout << "Waiting for up to 10 messages with 3s timeout..." << std::endl;
    try {
        auto messages = co_await queue->async_read_msgs_with_timeout(10, 3s, use_awaitable);
        std::cout << "✅ Received " << messages.size() << " messages:" << std::endl;
        for (const auto& msg : messages) {
            std::cout << "  - #" << msg.id << ": " << msg.content << std::endl;
        }
    } catch (const std::system_error& e) {
        if (e.code() == std::errc::timed_out) {
            std::cout << "⏱️  Timeout! No messages received within 3 seconds." << std::endl;
        } else {
            std::cout << "❌ Error: " << e.what() << std::endl;
        }
    }
    
    std::cout << std::endl;
}

// Example 4: Retry with timeout
awaitable<void> example4_retry_with_timeout(
    std::shared_ptr<bcast::async_queue<Message>> queue)
{
    std::cout << "=== Example 4: Retry with Timeout ===" << std::endl;
    
    const int max_retries = 3;
    
    for (int retry = 0; retry < max_retries; ++retry) {
        std::cout << "Attempt " << (retry + 1) << "/" << max_retries << "..." << std::endl;
        
        try {
            auto msg = co_await queue->async_read_msg_with_timeout(1s, use_awaitable);
            std::cout << "  ✅ Success! Message: " << msg.content << std::endl;
            break;
        } catch (const std::system_error& e) {
            if (e.code() == std::errc::timed_out) {
                std::cout << "  ⏱️  Timeout, retrying..." << std::endl;
                continue;
            } else {
                std::cout << "  ❌ Error: " << e.what() << std::endl;
                break;
            }
        }
    }
    
    std::cout << std::endl;
}

// Example 5: Processing with timeout and fallback
awaitable<void> example5_timeout_with_fallback(
    std::shared_ptr<bcast::async_queue<Message>> queue)
{
    std::cout << "=== Example 5: Timeout with Fallback ===" << std::endl;
    
    while (true) {
        try {
            auto msg = co_await queue->async_read_msg_with_timeout(2s, use_awaitable);
            std::cout << "✅ Processing message: " << msg.content << std::endl;
        } catch (const std::system_error& e) {
            if (e.code() == std::errc::timed_out) {
                std::cout << "⏱️  No message received, performing periodic task..." << std::endl;
                // Do some periodic work
                std::cout << "   💼 Executing fallback logic" << std::endl;
                
                // Exit after one timeout for demo
                break;
            } else {
                std::cout << "❌ Error: " << e.what() << std::endl;
                break;
            }
        }
    }
    
    std::cout << std::endl;
}

// Delayed publisher - publishes messages after delays
awaitable<void> delayed_publisher(
    std::shared_ptr<bcast::dispatcher<Message>> disp,
    std::vector<std::pair<std::chrono::milliseconds, Message>> schedule)
{
    auto executor = co_await asio::this_coro::executor;
    asio::steady_timer timer(executor);
    
    for (const auto& [delay, msg] : schedule) {
        timer.expires_after(delay);
        co_await timer.async_wait(use_awaitable);
        
        std::cout << "[Publisher] Publishing message #" << msg.id << std::endl;
        disp->publish(msg);
    }
}

awaitable<void> run_examples(asio::io_context& io)
{
    auto dispatcher = bcast::make_dispatcher<Message>(io);
    
    // Example 1: Timeout (no messages)
    {
        auto queue = dispatcher->subscribe();
        co_await example1_simple_timeout(queue);
        dispatcher->unsubscribe(queue);
    }
    
    // Example 2: Some timeouts, some messages
    {
        auto queue = dispatcher->subscribe();
        
        // Schedule some messages
        std::vector<std::pair<std::chrono::milliseconds, Message>> schedule = {
            {500ms, {1, "First message"}},
            {2500ms, {2, "Second message (after long delay)"}},
            {3000ms, {3, "Third message"}},
        };
        
        co_spawn(io, delayed_publisher(dispatcher, schedule), detached);
        co_await example2_multiple_timeouts(queue);
        
        dispatcher->unsubscribe(queue);
    }
    
    // Example 3: Batch with timeout
    {
        auto queue = dispatcher->subscribe();
        
        // Publish a few messages quickly
        auto executor = co_await asio::this_coro::executor;
        asio::steady_timer timer(executor);
        timer.expires_after(500ms);
        co_await timer.async_wait(use_awaitable);
        
        for (int i = 1; i <= 3; ++i) {
            dispatcher->publish(Message{i, "Batch message " + std::to_string(i)});
        }
        
        co_await example3_batch_timeout(queue);
        dispatcher->unsubscribe(queue);
    }
    
    // Example 4: Retry
    {
        auto queue = dispatcher->subscribe();
        
        // Publish after 2.5 seconds (will succeed on 3rd retry)
        auto executor = co_await asio::this_coro::executor;
        asio::steady_timer timer(executor);
        timer.expires_after(2500ms);
        co_await timer.async_wait(use_awaitable);
        dispatcher->publish(Message{100, "Finally arrived!"});
        
        co_await example4_retry_with_timeout(queue);
        dispatcher->unsubscribe(queue);
    }
    
    // Example 5: Fallback
    {
        auto queue = dispatcher->subscribe();
        co_await example5_timeout_with_fallback(queue);
        dispatcher->unsubscribe(queue);
    }
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  Timeout Examples for Async Queue" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;
    
    try {
        asio::io_context io;
        
        co_spawn(io, run_examples(io), detached);
        
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

