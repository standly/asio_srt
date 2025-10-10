//
// Example demonstrating async publish-subscribe pattern
//

#include "dispatcher.hpp"
#include <asio.hpp>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>

// Example message structure
struct Message {
    std::string topic;
    std::string content;
    int priority;
    
    Message(std::string t, std::string c, int p = 0)
        : topic(std::move(t)), content(std::move(c)), priority(p) {}
};

int main() {
    try {
        // Create ASIO io_context
        asio::io_context io_context;
        
        // Create dispatcher for Message type
        auto msg_dispatcher = bcast::make_dispatcher<Message>(io_context);
        
        std::cout << "=== Asynchronous Publish-Subscribe Example ===" << std::endl;
        std::cout << "Using ASIO strand for lock-free operations" << std::endl << std::endl;
        
        // Subscribe first subscriber
        auto sub1_id = msg_dispatcher->subscribe([](const Message& msg) {
            std::cout << "[Subscriber 1] Topic: " << msg.topic 
                      << ", Content: " << msg.content 
                      << ", Priority: " << msg.priority << std::endl;
        });
        
        // Subscribe second subscriber with delay to simulate slow processing
        auto sub2_id = msg_dispatcher->subscribe([](const Message& msg) {
            // Simulate some processing time
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            std::cout << "[Subscriber 2 - Slow] Processed: " << msg.content << std::endl;
        });
        
        // Subscribe third subscriber that filters by priority
        auto sub3_id = msg_dispatcher->subscribe([](const Message& msg) {
            if (msg.priority >= 5) {
                std::cout << "[Subscriber 3 - High Priority Only] !!! " 
                          << msg.content << " !!!" << std::endl;
            }
        });
        
        // Publish some messages from main thread
        std::cout << "Publishing messages from main thread..." << std::endl;
        msg_dispatcher->publish(Message("news", "Breaking news!", 10));
        msg_dispatcher->publish(Message("sports", "Game update", 3));
        msg_dispatcher->publish(Message("weather", "Sunny day", 1));
        
        // Create worker threads that also publish messages
        std::thread publisher_thread([&]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            std::cout << "\nPublishing from worker thread..." << std::endl;
            msg_dispatcher->publish(Message("tech", "New release", 7));
            msg_dispatcher->publish(Message("business", "Market update", 4));
        });
        
        // Check subscriber count asynchronously
        msg_dispatcher->get_subscriber_count([](size_t count) {
            std::cout << "\nCurrent subscribers: " << count << std::endl;
        });
        
        // Run io_context in multiple threads to demonstrate thread-safety
        std::vector<std::thread> threads;
        for (int i = 0; i < 3; ++i) {
            threads.emplace_back([&io_context]() {
                io_context.run();
            });
        }
        
        // Wait for publisher thread
        publisher_thread.join();
        
        // Give some time for all messages to be processed
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        // Unsubscribe one subscriber
        std::cout << "\nUnsubscribing Subscriber 2..." << std::endl;
        msg_dispatcher->unsubscribe(sub2_id);
        
        // Publish more messages
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        std::cout << "Publishing after unsubscribe..." << std::endl;
        msg_dispatcher->publish(Message("alert", "System notification", 8));
        
        // Final subscriber count
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        msg_dispatcher->get_subscriber_count([](size_t count) {
            std::cout << "\nFinal subscribers: " << count << std::endl;
        });
        
        // Let everything finish
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Stop io_context
        io_context.stop();
        
        // Wait for all threads
        for (auto& t : threads) {
            if (t.joinable()) {
                t.join();
            }
        }
        
        std::cout << "\n=== Example completed ===" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}

