//
// Advanced example: Multi-topic dispatcher with routing and filtering
//

#include "dispatcher.hpp"
#include <asio.hpp>
#include <iostream>
#include <string>
#include <memory>
#include <thread>
#include <chrono>
#include <map>
#include <vector>

// Message with topic routing
struct TopicMessage {
    std::string topic;
    std::string payload;
    std::chrono::system_clock::time_point timestamp;
    
    TopicMessage(std::string t, std::string p)
        : topic(std::move(t))
        , payload(std::move(p))
        , timestamp(std::chrono::system_clock::now())
    {}
};

// Multi-topic dispatcher that routes messages to topic-specific handlers
class MultiTopicDispatcher {
public:
    explicit MultiTopicDispatcher(asio::io_context& io_context)
        : io_context_(io_context)
        , main_dispatcher_(bcast::make_dispatcher<TopicMessage>(io_context))
    {}
    
    // Subscribe to a specific topic
    uint64_t subscribe(const std::string& topic, 
                      std::function<void(const TopicMessage&)> handler) {
        auto id = next_id_++;
        
        // Subscribe to main dispatcher with filtering
        auto sub_id = main_dispatcher_->subscribe(
            [topic, handler = std::move(handler)](const TopicMessage& msg) {
                if (msg.topic == topic) {
                    handler(msg);
                }
            });
        
        subscriptions_[id] = sub_id;
        return id;
    }
    
    // Subscribe to multiple topics with wildcard pattern
    uint64_t subscribe_pattern(const std::string& pattern,
                               std::function<void(const TopicMessage&)> handler) {
        auto id = next_id_++;
        
        auto sub_id = main_dispatcher_->subscribe(
            [pattern, handler = std::move(handler)](const TopicMessage& msg) {
                // Simple wildcard matching: "topic.*" matches "topic.sub1", "topic.sub2", etc.
                if (matches_pattern(msg.topic, pattern)) {
                    handler(msg);
                }
            });
        
        subscriptions_[id] = sub_id;
        return id;
    }
    
    void unsubscribe(uint64_t id) {
        auto it = subscriptions_.find(id);
        if (it != subscriptions_.end()) {
            main_dispatcher_->unsubscribe(it->second);
            subscriptions_.erase(it);
        }
    }
    
    void publish(const std::string& topic, const std::string& payload) {
        main_dispatcher_->publish(TopicMessage(topic, payload));
    }
    
    void get_stats(std::function<void(size_t)> callback) {
        main_dispatcher_->get_subscriber_count(std::move(callback));
    }
    
private:
    static bool matches_pattern(const std::string& topic, const std::string& pattern) {
        // Simple wildcard matching
        if (pattern.find('*') == std::string::npos) {
            return topic == pattern;
        }
        
        size_t star_pos = pattern.find('*');
        std::string prefix = pattern.substr(0, star_pos);
        
        return topic.substr(0, prefix.length()) == prefix;
    }
    
    asio::io_context& io_context_;
    std::shared_ptr<bcast::dispatcher<TopicMessage>> main_dispatcher_;
    std::map<uint64_t, uint64_t> subscriptions_;
    uint64_t next_id_ = 1;
};

// Example: Priority-based message processing
struct PriorityMessage {
    int priority;  // Higher = more important
    std::string content;
    
    PriorityMessage(int p, std::string c)
        : priority(p), content(std::move(c)) {}
};

class PriorityDispatcher {
public:
    explicit PriorityDispatcher(asio::io_context& io_context)
        : io_context_(io_context)
    {
        // Create separate dispatchers for different priority levels
        high_priority_ = bcast::make_dispatcher<PriorityMessage>(io_context);
        medium_priority_ = bcast::make_dispatcher<PriorityMessage>(io_context);
        low_priority_ = bcast::make_dispatcher<PriorityMessage>(io_context);
    }
    
    uint64_t subscribe_priority(int min_priority,
                                std::function<void(const PriorityMessage&)> handler) {
        if (min_priority >= 8) {
            return high_priority_->subscribe(std::move(handler));
        } else if (min_priority >= 5) {
            return medium_priority_->subscribe(std::move(handler));
        } else {
            return low_priority_->subscribe(std::move(handler));
        }
    }
    
    void publish(const PriorityMessage& msg) {
        if (msg.priority >= 8) {
            high_priority_->publish(msg);
        } else if (msg.priority >= 5) {
            medium_priority_->publish(msg);
        } else {
            low_priority_->publish(msg);
        }
    }
    
private:
    asio::io_context& io_context_;
    std::shared_ptr<bcast::dispatcher<PriorityMessage>> high_priority_;
    std::shared_ptr<bcast::dispatcher<PriorityMessage>> medium_priority_;
    std::shared_ptr<bcast::dispatcher<PriorityMessage>> low_priority_;
};

int main() {
    std::cout << "=== Advanced Publish-Subscribe Examples ===" << std::endl << std::endl;
    
    try {
        asio::io_context io_context;
        
        // Example 1: Multi-topic dispatcher
        {
            std::cout << "Example 1: Multi-topic dispatcher with routing" << std::endl;
            std::cout << "----------------------------------------------" << std::endl;
            
            MultiTopicDispatcher dispatcher(io_context);
            
            // Subscribe to specific topics
            auto sub1 = dispatcher.subscribe("sensor.temperature", 
                [](const TopicMessage& msg) {
                    std::cout << "[Temperature] " << msg.payload << std::endl;
                });
            
            auto sub2 = dispatcher.subscribe("sensor.humidity",
                [](const TopicMessage& msg) {
                    std::cout << "[Humidity] " << msg.payload << std::endl;
                });
            
            // Subscribe with wildcard pattern
            auto sub3 = dispatcher.subscribe_pattern("sensor.*",
                [](const TopicMessage& msg) {
                    auto now = std::chrono::system_clock::now();
                    auto latency = std::chrono::duration_cast<std::chrono::microseconds>(
                        now - msg.timestamp).count();
                    std::cout << "[All Sensors] " << msg.topic << ": " << msg.payload 
                              << " (latency: " << latency << "μs)" << std::endl;
                });
            
            // Publish messages
            dispatcher.publish("sensor.temperature", "25.5°C");
            dispatcher.publish("sensor.humidity", "65%");
            dispatcher.publish("sensor.temperature", "26.0°C");
            dispatcher.publish("system.status", "OK");  // No subscribers
            
            // Run io_context briefly
            io_context.run_for(std::chrono::milliseconds(100));
            io_context.restart();
            
            std::cout << std::endl;
        }
        
        // Example 2: Priority-based processing
        {
            std::cout << "Example 2: Priority-based message processing" << std::endl;
            std::cout << "---------------------------------------------" << std::endl;
            
            PriorityDispatcher dispatcher(io_context);
            
            // Subscribe to high priority only
            auto sub_high = dispatcher.subscribe_priority(8,
                [](const PriorityMessage& msg) {
                    std::cout << "[HIGH] " << msg.content << " (priority: " 
                              << msg.priority << ")" << std::endl;
                });
            
            // Subscribe to medium+ priority
            auto sub_med = dispatcher.subscribe_priority(5,
                [](const PriorityMessage& msg) {
                    std::cout << "[MEDIUM+] " << msg.content << " (priority: " 
                              << msg.priority << ")" << std::endl;
                });
            
            // Subscribe to all priorities
            auto sub_all = dispatcher.subscribe_priority(0,
                [](const PriorityMessage& msg) {
                    std::cout << "[ALL] " << msg.content << " (priority: " 
                              << msg.priority << ")" << std::endl;
                });
            
            // Publish messages with different priorities
            dispatcher.publish(PriorityMessage(10, "Critical alert!"));
            dispatcher.publish(PriorityMessage(6, "Important update"));
            dispatcher.publish(PriorityMessage(2, "Info message"));
            dispatcher.publish(PriorityMessage(9, "Urgent notification"));
            
            io_context.run_for(std::chrono::milliseconds(100));
            io_context.restart();
            
            std::cout << std::endl;
        }
        
        // Example 3: Message batching and aggregation
        {
            std::cout << "Example 3: Message batching and aggregation" << std::endl;
            std::cout << "--------------------------------------------" << std::endl;
            
            auto dispatcher = bcast::make_dispatcher<int>(io_context);
            
            // Aggregating subscriber
            std::vector<int> batch;
            const size_t batch_size = 5;
            
            auto sub = dispatcher->subscribe([&batch, batch_size](int value) {
                batch.push_back(value);
                
                if (batch.size() >= batch_size) {
                    // Process batch
                    int sum = 0;
                    for (int v : batch) sum += v;
                    std::cout << "Batch processed: " << batch.size() << " items, sum=" 
                              << sum << std::endl;
                    batch.clear();
                }
            });
            
            // Publish values
            for (int i = 1; i <= 12; ++i) {
                dispatcher->publish(i);
            }
            
            io_context.run_for(std::chrono::milliseconds(100));
            io_context.restart();
            
            std::cout << std::endl;
        }
        
        // Example 4: Chain of responsibility / pipeline
        {
            std::cout << "Example 4: Processing pipeline" << std::endl;
            std::cout << "--------------------------------" << std::endl;
            
            // Stage 1: Raw data dispatcher
            auto stage1 = bcast::make_dispatcher<std::string>(io_context);
            
            // Stage 2: Processed data dispatcher
            auto stage2 = bcast::make_dispatcher<std::string>(io_context);
            
            // Connect stage 1 to stage 2 with transformation
            stage1->subscribe([&stage2](const std::string& raw) {
                std::string processed = "[Validated] " + raw;
                std::cout << "Stage 1 -> Stage 2: " << processed << std::endl;
                stage2->publish(processed);
            });
            
            // Final consumer
            stage2->subscribe([](const std::string& final) {
                std::cout << "Final output: " << final << std::endl;
            });
            
            // Push raw data
            stage1->publish("raw_data_1");
            stage1->publish("raw_data_2");
            
            io_context.run_for(std::chrono::milliseconds(100));
            
            std::cout << std::endl;
        }
        
        std::cout << "=== All examples completed ===" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}

