//
// Created by ubuntu on 10/10/25.
//

#pragma once

#include "async_queue.hpp"
#include <asio.hpp>
#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/use_awaitable.hpp>
#include <memory>
#include <unordered_map>
#include <vector>
#include <atomic>
#include <functional>

namespace bcast {

/**
 * @brief Dispatcher for publish-subscribe pattern using ASIO strand
 * 
 * This dispatcher manages subscribers and broadcasts messages to them
 * in a thread-safe, lock-free manner using strand for serialization.
 * Each subscriber gets its own async_queue for message processing.
 * 
 * Usage:
 *   auto disp = bcast::make_dispatcher<Message>(io_context);
 *   auto queue = disp->subscribe();  // Get a queue
 *   
 *   // In publisher:
 *   disp->publish(message);
 *   
 *   // In subscriber (coroutine):
 *   while (true) {
 *       auto msg = co_await queue->async_read_msg(asio::use_awaitable);
 *       process(msg);
 *   }
 * 
 * @tparam T Message type
 */
template <typename T>
class dispatcher : public std::enable_shared_from_this<dispatcher<T>> {
public:
    using queue_ptr = std::shared_ptr<async_queue<T>>;

    /**
     * @brief Construct dispatcher
     * @param io_context ASIO io_context for async operations
     */
    explicit dispatcher(asio::io_context& io_context)
        : strand_(asio::make_strand(io_context))
        , io_context_(io_context)
        , next_id_(1)
    {
    }

    /**
     * @brief Subscribe to messages and get an async_queue
     * @return Shared pointer to async_queue for reading messages
     * 
     * Thread-safe: can be called from any thread.
     * 
     * The returned queue can be used with:
     * - co_await queue->async_read_msg(asio::use_awaitable)  // Read single message
     * - co_await queue->async_read_msgs(10, asio::use_awaitable)  // Read up to 10 messages
     */
    queue_ptr subscribe() {
        auto queue = std::make_shared<async_queue<T>>(io_context_);
        uint64_t id = next_id_.fetch_add(1, std::memory_order_relaxed);
        
        asio::post(strand_, [self = this->shared_from_this(), id, queue]() {
            self->subscribers_[id] = queue;
            self->queue_to_id_[queue.get()] = id;
        });
        
        return queue;
    }

    /**
     * @brief Subscribe with a callback function (for backward compatibility)
     * @param handler Callback function to process messages
     * @return Subscriber ID that can be used with unsubscribe_by_id()
     * 
     * Thread-safe: can be called from any thread.
     * 
     * This method internally uses a recursive async read pattern to process messages.
     */
    uint64_t subscribe(std::function<void(const T&)> handler) {
        auto queue = std::make_shared<async_queue<T>>(io_context_);
        uint64_t id = next_id_.fetch_add(1, std::memory_order_relaxed);
        
        asio::post(strand_, [self = this->shared_from_this(), id, queue]() {
            self->subscribers_[id] = queue;
            self->queue_to_id_[queue.get()] = id;
        });
        
        // Start reading messages recursively
        start_reading(queue, std::move(handler));
        
        return id;
    }

    /**
     * @brief Unsubscribe by subscriber ID
     * @param id The subscriber ID returned from subscribe(handler)
     * 
     * Thread-safe: can be called from any thread
     */
    void unsubscribe_by_id(uint64_t id) {
        asio::post(strand_, [self = this->shared_from_this(), id]() {
            auto it = self->subscribers_.find(id);
            if (it != self->subscribers_.end()) {
                it->second->stop();
                
                // Also remove from queue_to_id_ map
                auto queue_ptr = it->second.get();
                self->queue_to_id_.erase(queue_ptr);
                
                self->subscribers_.erase(it);
            }
        });
    }

    /**
     * @brief Unsubscribe a queue from receiving messages
     * @param queue The queue returned from subscribe()
     * 
     * Thread-safe: can be called from any thread
     */
    void unsubscribe(queue_ptr queue) {
        asio::post(strand_, [self = this->shared_from_this(), queue]() {
            auto it = self->queue_to_id_.find(queue.get());
            if (it != self->queue_to_id_.end()) {
                uint64_t id = it->second;
                self->queue_to_id_.erase(it);
                
                auto sub_it = self->subscribers_.find(id);
                if (sub_it != self->subscribers_.end()) {
                    sub_it->second->stop();
                    self->subscribers_.erase(sub_it);
                }
            }
        });
    }

    /**
     * @brief Publish message to all subscribers
     * @param msg Message to broadcast
     * 
     * Thread-safe: can be called from any thread.
     * Messages are delivered asynchronously to each subscriber's queue.
     */
    void publish(const T& msg) {
        asio::post(strand_, [self = this->shared_from_this(), msg]() {
            for (auto& [id, queue] : self->subscribers_) {
                // Each subscriber gets a copy of the message
                queue->push(msg);
            }
        });
    }

    /**
     * @brief Publish message to all subscribers (rvalue version)
     * @param msg Message to broadcast
     */
    void publish(T&& msg) {
        asio::post(strand_, [self = this->shared_from_this(), msg = std::move(msg)]() mutable {
            // For the last subscriber, we can move; for others, copy
            if (self->subscribers_.empty()) {
                return;
            }
            
            auto it = self->subscribers_.begin();
            auto last = std::prev(self->subscribers_.end());
            
            // Copy to all but last
            for (; it != last; ++it) {
                it->second->push(msg);
            }
            
            // Move to last
            last->second->push(std::move(msg));
        });
    }

    /**
     * @brief Publish multiple messages to all subscribers (batch operation)
     * @param messages Vector of messages to broadcast
     * 
     * Thread-safe: can be called from any thread.
     * More efficient than calling publish() multiple times.
     * Each subscriber receives all messages.
     * 
     * Usage:
     *   std::vector<Message> batch = {msg1, msg2, msg3};
     *   dispatcher->publish_batch(batch);
     */
    void publish_batch(std::vector<T> messages) {
        if (messages.empty()) {
            return;
        }
        
        asio::post(strand_, [self = this->shared_from_this(), messages = std::move(messages)]() {
            for (auto& [id, queue] : self->subscribers_) {
                // Each subscriber gets a copy of all messages
                queue->push_batch(messages);
            }
        });
    }

    /**
     * @brief Publish multiple messages using iterators
     * @param begin Iterator to first message
     * @param end Iterator past last message
     * 
     * Thread-safe: can be called from any thread
     * 
     * Usage:
     *   std::vector<Message> msgs = {msg1, msg2, msg3};
     *   dispatcher->publish_batch(msgs.begin(), msgs.end());
     */
    template<typename Iterator>
    void publish_batch(Iterator begin, Iterator end) {
        if (begin == end) {
            return;
        }
        
        std::vector<T> messages(begin, end);
        publish_batch(std::move(messages));
    }

    /**
     * @brief Publish multiple messages from initializer list
     * @param init_list Initializer list of messages
     * 
     * Thread-safe: can be called from any thread
     * 
     * Usage:
     *   dispatcher->publish_batch({msg1, msg2, msg3});
     */
    void publish_batch(std::initializer_list<T> init_list) {
        if (init_list.size() == 0) {
            return;
        }
        
        std::vector<T> messages(init_list);
        publish_batch(std::move(messages));
    }

    /**
     * @brief Get subscriber count (async)
     * @param callback Callback with count (called on strand)
     */
    void get_subscriber_count(std::function<void(size_t)> callback) {
        asio::post(strand_, [self = this->shared_from_this(), callback = std::move(callback)]() {
            callback(self->subscribers_.size());
        });
    }

    /**
     * @brief Get subscriber count (synchronous)
     * Note: This is a snapshot and may be outdated immediately
     */
    size_t subscriber_count() const {
        return subscribers_.size();
    }

    /**
     * @brief Clear all subscribers
     * 
     * Thread-safe: can be called from any thread
     */
    void clear() {
        asio::post(strand_, [self = this->shared_from_this()]() {
            for (auto& [id, queue] : self->subscribers_) {
                queue->stop();
            }
            self->subscribers_.clear();
            self->queue_to_id_.clear();
        });
    }

private:
    /**
     * @brief Helper method to recursively read messages from queue
     */
    void start_reading(queue_ptr queue, std::function<void(const T&)> handler) {
        queue->async_read_msg(
            [self = this->shared_from_this(), queue, handler](std::error_code ec, T msg) {
                if (ec) {
                    // Queue stopped, end the reading loop
                    return;
                }
                
                // Call user handler
                handler(msg);
                
                // Continue reading
                self->start_reading(queue, handler);
            }
        );
    }

    asio::strand<asio::io_context::executor_type> strand_;
    asio::io_context& io_context_;
    std::unordered_map<uint64_t, queue_ptr> subscribers_;
    std::unordered_map<async_queue<T>*, uint64_t> queue_to_id_;
    std::atomic<uint64_t> next_id_;
};

/**
 * @brief Factory function to create a dispatcher
 * @param io_context ASIO io_context
 * @return Shared pointer to dispatcher
 */
template <typename T>
std::shared_ptr<dispatcher<T>> make_dispatcher(asio::io_context& io_context) {
    return std::make_shared<dispatcher<T>>(io_context);
}

} // namespace bcast
