//
// Coroutine example: Simple publish-subscribe using co_await
// Requires C++20 and ASIO with coroutine support
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

// Simple message structure
struct Message {
    int id;
    std::string content;
    int priority;
    
    Message() : id(0), content(""), priority(0) {}
    Message(int i, std::string c, int p) : id(i), content(std::move(c)), priority(p) {}
};

// Subscriber coroutine - reads messages in a loop
awaitable<void> subscriber_task(
    std::shared_ptr<bcast::async_queue<Message>> queue,
    std::string name)
{
    std::cout << "[" << name << "] Started" << std::endl;
    
    try {
        while (!queue->is_stopped()) {
            // Co_await to read a single message - so simple!
            auto msg = co_await queue->async_read_msg(use_awaitable);
            
            std::cout << "[" << name << "] Received: #" << msg.id 
                      << " - " << msg.content 
                      << " (priority: " << msg.priority << ")" 
                      << std::endl;
        }
    } catch (const std::system_error& e) {
        // Queue stopped or error occurred
        std::cout << "[" << name << "] Stopped: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cout << "[" << name << "] Exception: " << e.what() << std::endl;
    }
    
    std::cout << "[" << name << "] Finished" << std::endl;
}

// Batch subscriber - reads multiple messages at once
awaitable<void> batch_subscriber_task(
    std::shared_ptr<bcast::async_queue<Message>> queue,
    std::string name)
{
    std::cout << "[" << name << "] Started (batch mode)" << std::endl;
    
    try {
        while (!queue->is_stopped()) {
            // Read up to 5 messages at once
            auto messages = co_await queue->async_read_msgs(5, use_awaitable);
            
            if (!messages.empty()) {
                std::cout << "[" << name << "] Received batch of " << messages.size() << " messages:" << std::endl;
                for (const auto& msg : messages) {
                    std::cout << "  - #" << msg.id << ": " << msg.content << std::endl;
                }
            }
        }
    } catch (const std::system_error& e) {
        // Queue stopped or error occurred
        std::cout << "[" << name << "] Stopped: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cout << "[" << name << "] Exception: " << e.what() << std::endl;
    }
    
    std::cout << "[" << name << "] Finished" << std::endl;
}

// Publisher coroutine
awaitable<void> publisher_task(std::shared_ptr<bcast::dispatcher<Message>> disp)
{
    std::cout << "[Publisher] Started" << std::endl;
    
    auto executor = co_await asio::this_coro::executor;
    asio::steady_timer timer(executor);
    
    for (int i = 1; i <= 10; ++i) {
        Message msg{i, "Message " + std::to_string(i), i % 3};
        
        std::cout << "[Publisher] Publishing #" << i << std::endl;
        disp->publish(msg);
        
        // Wait a bit between messages
        timer.expires_after(100ms);
        co_await timer.async_wait(use_awaitable);
    }
    
    std::cout << "[Publisher] Finished publishing" << std::endl;
}

// Main function demonstrating the simple API
awaitable<void> async_main(std::shared_ptr<bcast::dispatcher<Message>> dispatcher)
{
    std::cout << "=== Coroutine Publish-Subscribe Example ===" << std::endl;
    std::cout << "Using co_await for simple async message reading" << std::endl << std::endl;
    
    // Subscribe - get queues for reading messages
    auto queue1 = dispatcher->subscribe();
    auto queue2 = dispatcher->subscribe();
    auto queue3 = dispatcher->subscribe();
    
    std::cout << "Created 3 subscribers" << std::endl << std::endl;
    
    // Spawn subscriber coroutines
    co_spawn(
        co_await asio::this_coro::executor,
        subscriber_task(queue1, "Subscriber-1"),
        detached
    );
    
    co_spawn(
        co_await asio::this_coro::executor,
        subscriber_task(queue2, "Subscriber-2"),
        detached
    );
    
    co_spawn(
        co_await asio::this_coro::executor,
        batch_subscriber_task(queue3, "BatchSubscriber"),
        detached
    );
    
    // Wait a bit for subscribers to start
    auto executor = co_await asio::this_coro::executor;
    asio::steady_timer timer(executor);
    timer.expires_after(100ms);
    co_await timer.async_wait(use_awaitable);
    
    // Spawn publisher
    co_await publisher_task(dispatcher);
    
    // Wait for all messages to be processed
    timer.expires_after(500ms);
    co_await timer.async_wait(use_awaitable);
    
    // Cleanup
    std::cout << "\nCleaning up..." << std::endl;
    dispatcher->clear();
    
    // Give time for cleanup
    timer.expires_after(100ms);
    co_await timer.async_wait(use_awaitable);
    
    std::cout << "\n=== Example Completed ===" << std::endl;
}

// Comparison: Old callback style vs New coroutine style
void demonstrate_api_comparison(asio::io_context& io_context)
{
    std::cout << "\n=== API Comparison ===" << std::endl;
    std::cout << "\nOLD STYLE (Callback-based):" << std::endl;
    std::cout << "  auto sub_id = dispatcher->subscribe([](const Message& msg) {" << std::endl;
    std::cout << "      // Handle message" << std::endl;
    std::cout << "      process(msg);" << std::endl;
    std::cout << "  });" << std::endl;
    
    std::cout << "\nNEW STYLE (Coroutine-based):" << std::endl;
    std::cout << "  auto queue = dispatcher->subscribe();" << std::endl;
    std::cout << "  co_spawn(executor, [queue]() -> awaitable<void> {" << std::endl;
    std::cout << "      while (true) {" << std::endl;
    std::cout << "          auto [ec, msg] = co_await queue->async_read_msg(use_awaitable);" << std::endl;
    std::cout << "          if (ec) break;" << std::endl;
    std::cout << "          process(msg);" << std::endl;
    std::cout << "      }" << std::endl;
    std::cout << "  }, detached);" << std::endl;
    
    std::cout << "\nBENEFITS:" << std::endl;
    std::cout << "  ✓ Write async code in synchronous style" << std::endl;
    std::cout << "  ✓ Easy to add control flow (loops, conditions, try-catch)" << std::endl;
    std::cout << "  ✓ Better error handling" << std::endl;
    std::cout << "  ✓ No callback hell" << std::endl;
    std::cout << std::endl;
}

int main() {
    try {
        asio::io_context io_context;
        
        // Create dispatcher
        auto dispatcher = bcast::make_dispatcher<Message>(io_context);
        
        // Demonstrate API comparison
        demonstrate_api_comparison(io_context);
        
        // Run the async main
        co_spawn(io_context, async_main(dispatcher), detached);
        
        // Run io_context
        io_context.run();
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}

