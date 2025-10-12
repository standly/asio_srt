//
// Real-world example: WebSocket-like message broadcasting with coroutines
//

#include "bcast/dispatcher.hpp"
#include <asio.hpp>
#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/use_awaitable.hpp>
#include <iostream>
#include <string>
#include <chrono>
#include <memory>
#include <map>

using asio::awaitable;
using asio::co_spawn;
using asio::detached;
using asio::use_awaitable;
using namespace std::chrono_literals;

// Event types for a chat application
enum class EventType {
    USER_JOINED,
    USER_LEFT,
    MESSAGE,
    TYPING,
    SYSTEM
};

struct ChatEvent {
    EventType type;
    std::string user_id;
    std::string content;
    int64_t timestamp;
    
    ChatEvent() : type(EventType::USER_JOINED), user_id(""), content(""), timestamp(0) {}
    ChatEvent(EventType t, std::string u, std::string c)
        : type(t)
        , user_id(std::move(u))
        , content(std::move(c))
        , timestamp(std::chrono::system_clock::now().time_since_epoch().count())
    {}
};

// Simulated chat room
class ChatRoom {
public:
    explicit ChatRoom(asio::io_context& io_context, std::string name)
        : name_(std::move(name))
        , dispatcher_(bcast::make_dispatcher<ChatEvent>(io_context))
        , io_context_(io_context)
    {
        std::cout << "[ChatRoom:" << name_ << "] Created" << std::endl;
    }
    
    // User joins the chat room - returns a queue for receiving events
    std::shared_ptr<bcast::async_queue<ChatEvent>> join(const std::string& user_id) {
        auto queue = dispatcher_->subscribe();
        
        // Broadcast join event
        dispatcher_->publish(ChatEvent{
            EventType::USER_JOINED,
            user_id,
            user_id + " has joined the room"
        });
        
        std::cout << "[ChatRoom:" << name_ << "] " << user_id << " joined" << std::endl;
        return queue;
    }
    
    // User leaves
    void leave(std::shared_ptr<bcast::async_queue<ChatEvent>> queue, const std::string& user_id) {
        // Broadcast leave event first
        dispatcher_->publish(ChatEvent{
            EventType::USER_LEFT,
            user_id,
            user_id + " has left the room"
        });
        
        // Then unsubscribe
        dispatcher_->unsubscribe(queue);
        
        std::cout << "[ChatRoom:" << name_ << "] " << user_id << " left" << std::endl;
    }
    
    // Broadcast a message
    void broadcast(const ChatEvent& event) {
        dispatcher_->publish(event);
    }
    
    size_t user_count() const {
        return dispatcher_->subscriber_count();
    }
    
private:
    std::string name_;
    std::shared_ptr<bcast::dispatcher<ChatEvent>> dispatcher_;
    asio::io_context& io_context_;
};

// Simulated chat user with coroutine-based message handling
class ChatUser {
public:
    ChatUser(std::string id, std::shared_ptr<ChatRoom> room)
        : user_id_(std::move(id))
        , room_(std::move(room))
        , queue_(nullptr)
        , active_(false)
    {}
    
    // Connect to the room and start receiving messages
    awaitable<void> connect() {
        queue_ = room_->join(user_id_);
        active_ = true;
        
        std::cout << "[User:" << user_id_ << "] Connected" << std::endl;
        
        // Read messages in a loop
        try {
            while (active_) {
                auto event = co_await queue_->async_read_msg(use_awaitable);
                handle_event(event);
            }
        } catch (const std::system_error& e) {
            // Queue stopped
            std::cout << "[User:" << user_id_ << "] Disconnected: " << e.what() << std::endl;
        } catch (const std::exception& e) {
            std::cout << "[User:" << user_id_ << "] Exception: " << e.what() << std::endl;
        }
    }
    
    // Send a message
    void send_message(const std::string& content) {
        if (active_) {
            room_->broadcast(ChatEvent{
                EventType::MESSAGE,
                user_id_,
                content
            });
        }
    }
    
    // Disconnect
    void disconnect() {
        if (active_ && queue_) {
            active_ = false;
            room_->leave(queue_, user_id_);
        }
    }
    
private:
    void handle_event(const ChatEvent& event) {
        // Don't show own messages
        if (event.user_id == user_id_ && event.type == EventType::MESSAGE) {
            return;
        }
        
        switch (event.type) {
            case EventType::USER_JOINED:
                std::cout << "[User:" << user_id_ << "] 👋 " << event.content << std::endl;
                break;
                
            case EventType::USER_LEFT:
                std::cout << "[User:" << user_id_ << "] 👋 " << event.content << std::endl;
                break;
                
            case EventType::MESSAGE:
                std::cout << "[User:" << user_id_ << "] 💬 " << event.user_id 
                          << ": " << event.content << std::endl;
                break;
                
            case EventType::TYPING:
                std::cout << "[User:" << user_id_ << "] ✏️  " << event.user_id 
                          << " is typing..." << std::endl;
                break;
                
            case EventType::SYSTEM:
                std::cout << "[User:" << user_id_ << "] 🔔 System: " << event.content << std::endl;
                break;
        }
    }
    
    std::string user_id_;
    std::shared_ptr<ChatRoom> room_;
    std::shared_ptr<bcast::async_queue<ChatEvent>> queue_;
    bool active_;
};

// Simulate chat activity
awaitable<void> simulate_chat_activity(std::shared_ptr<ChatRoom> room)
{
    auto executor = co_await asio::this_coro::executor;
    asio::steady_timer timer(executor);
    
    // Create users
    auto alice = std::make_shared<ChatUser>("Alice", room);
    auto bob = std::make_shared<ChatUser>("Bob", room);
    auto charlie = std::make_shared<ChatUser>("Charlie", room);
    
    // Connect users
    co_spawn(executor, alice->connect(), detached);
    timer.expires_after(200ms);
    co_await timer.async_wait(use_awaitable);
    
    co_spawn(executor, bob->connect(), detached);
    timer.expires_after(200ms);
    co_await timer.async_wait(use_awaitable);
    
    co_spawn(executor, charlie->connect(), detached);
    timer.expires_after(300ms);
    co_await timer.async_wait(use_awaitable);
    
    // Simulate conversation
    std::cout << "\n--- Chat conversation begins ---\n" << std::endl;
    
    alice->send_message("Hello everyone!");
    timer.expires_after(200ms);
    co_await timer.async_wait(use_awaitable);
    
    bob->send_message("Hey Alice! How are you?");
    timer.expires_after(200ms);
    co_await timer.async_wait(use_awaitable);
    
    charlie->send_message("Hi all! Great to be here!");
    timer.expires_after(200ms);
    co_await timer.async_wait(use_awaitable);
    
    // System announcement
    room->broadcast(ChatEvent{
        EventType::SYSTEM,
        "System",
        "Server maintenance in 1 hour"
    });
    timer.expires_after(300ms);
    co_await timer.async_wait(use_awaitable);
    
    alice->send_message("Thanks for the heads up!");
    timer.expires_after(200ms);
    co_await timer.async_wait(use_awaitable);
    
    // Bob leaves
    std::cout << "\n--- Bob disconnects ---\n" << std::endl;
    bob->disconnect();
    timer.expires_after(300ms);
    co_await timer.async_wait(use_awaitable);
    
    charlie->send_message("Where did Bob go?");
    timer.expires_after(200ms);
    co_await timer.async_wait(use_awaitable);
    
    alice->send_message("He probably had to leave. See you later!");
    timer.expires_after(500ms);
    co_await timer.async_wait(use_awaitable);
    
    // Cleanup
    std::cout << "\n--- Session ending ---\n" << std::endl;
    alice->disconnect();
    charlie->disconnect();
    
    timer.expires_after(200ms);
    co_await timer.async_wait(use_awaitable);
    
    std::cout << "\nFinal user count: " << room->user_count() << std::endl;
}

// Another example: Stock price updates
struct StockUpdate {
    std::string symbol;
    double price;
    int64_t volume;
    int64_t timestamp;
    
    StockUpdate() : symbol(""), price(0.0), volume(0), timestamp(0) {}
    StockUpdate(std::string s, double p, int64_t v)
        : symbol(std::move(s)), price(p), volume(v),
          timestamp(std::chrono::system_clock::now().time_since_epoch().count()) {}
};

awaitable<void> stock_subscriber_task(
    std::shared_ptr<bcast::async_queue<StockUpdate>> queue,
    std::string name)
{
    std::cout << "[" << name << "] Monitoring stock prices..." << std::endl;
    
    try {
        while (true) {
            // Read stock updates
            auto update = co_await queue->async_read_msg(use_awaitable);
            
            std::cout << "[" << name << "] " << update.symbol 
                      << ": $" << std::fixed << std::setprecision(2) << update.price
                      << " (vol: " << update.volume << ")" << std::endl;
            
            // Simple trading logic
            if (update.price < 100.0) {
                std::cout << "[" << name << "] 🟢 BUY signal for " << update.symbol << std::endl;
            } else if (update.price > 150.0) {
                std::cout << "[" << name << "] 🔴 SELL signal for " << update.symbol << std::endl;
            }
        }
    } catch (const std::system_error& e) {
        // Queue stopped
        std::cout << "[" << name << "] Stopped: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cout << "[" << name << "] Error: " << e.what() << std::endl;
    }
}

awaitable<void> stock_market_simulation(asio::io_context& io_context)
{
    std::cout << "\n\n=== Stock Market Example ===" << std::endl;
    
    auto stock_dispatcher = bcast::make_dispatcher<StockUpdate>(io_context);
    
    // Create trading bots
    auto bot1 = stock_dispatcher->subscribe();
    auto bot2 = stock_dispatcher->subscribe();
    
    auto executor = co_await asio::this_coro::executor;
    
    co_spawn(executor, stock_subscriber_task(bot1, "TradingBot-1"), detached);
    co_spawn(executor, stock_subscriber_task(bot2, "TradingBot-2"), detached);
    
    asio::steady_timer timer(executor);
    timer.expires_after(100ms);
    co_await timer.async_wait(use_awaitable);
    
    // Publish stock updates
    std::vector<std::string> symbols = {"AAPL", "GOOGL", "MSFT", "AMZN"};
    
    for (int i = 0; i < 8; ++i) {
        for (const auto& symbol : symbols) {
            double price = 90.0 + (i * 10) + (rand() % 20);
            
            stock_dispatcher->publish(StockUpdate{
                symbol,
                price,
                static_cast<int64_t>(100000 + rand() % 50000)
            });
        }
        
        timer.expires_after(150ms);
        co_await timer.async_wait(use_awaitable);
    }
    
    // Cleanup
    stock_dispatcher->clear();
    timer.expires_after(100ms);
    co_await timer.async_wait(use_awaitable);
}

int main() {
    try {
        std::cout << "=== Real-World Examples: Coroutine-based Pub-Sub ===" << std::endl;
        std::cout << "Simple API: dispatcher->publish() + co_await queue->async_read_msg()\n" << std::endl;
        
        asio::io_context io_context;
        
        // Example 1: Chat room
        std::cout << "=== Example 1: Chat Room ===" << std::endl;
        auto chat_room = std::make_shared<ChatRoom>(io_context, "General");
        co_spawn(io_context, simulate_chat_activity(chat_room), detached);
        
        // Run chat simulation
        io_context.run();
        
        // Reset for next example
        io_context.restart();
        
        // Example 2: Stock market
        co_spawn(io_context, stock_market_simulation(io_context), detached);
        io_context.run();
        
        std::cout << "\n=== All Examples Completed ===" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}

