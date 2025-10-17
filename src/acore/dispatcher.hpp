//
// Created by ubuntu on 10/10/25.
//

#pragma once

#include "async_queue.hpp"
#include <asio.hpp>
#include <memory>
#include <unordered_map>
#include <vector>
#include <atomic>

namespace acore {

/**
 * @brief Dispatcher for publish-subscribe pattern using ASIO strand and C++20 coroutines
 * 
 * This dispatcher manages subscribers and broadcasts messages to them
 * in a thread-safe, lock-free manner using strand for serialization.
 * Each subscriber gets its own async_queue for message processing.
 * 
 * Requires C++20 coroutines.
 * 
 * Usage:
 * @code
 * auto disp = acore::make_dispatcher<Message>(io_context);
 * auto queue = disp->subscribe();
 * 
 * // Subscriber coroutine
 * co_spawn(ex, [queue]() -> awaitable<void> {
 *     while (true) {
 *         auto [ec, msg] = co_await queue->async_read_msg(use_awaitable);
 *         if (ec) break;
 *         process(msg);
 *     }
 * }, detached);
 * 
 * // Publisher
 * disp->publish(message);
 * @endcode
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
     * Usage with coroutines:
     * @code
     * auto queue = dispatcher->subscribe();
     * co_spawn(ex, [queue]() -> awaitable<void> {
     *     while (true) {
     *         auto [ec, msg] = co_await queue->async_read_msg(use_awaitable);
     *         if (ec) break;
     *         process(msg);
     *     }
     * }, detached);
     * @endcode
     * 
     * Batch reading:
     * - co_await queue->async_read_msgs(10, asio::use_awaitable)  // Read up to 10 messages
     */
    queue_ptr subscribe() {
        auto queue = std::make_shared<async_queue<T>>(io_context_);
        uint64_t id = next_id_.fetch_add(1, std::memory_order_relaxed);
        
        asio::post(strand_, [self = this->shared_from_this(), id, queue]() {
            self->subscribers_[id] = queue;
            self->subscriber_count_.fetch_add(1, std::memory_order_relaxed);
        });
        
        return queue;
    }

    /**
     * @brief Unsubscribe by queue pointer
     * @param queue The queue returned from subscribe()
     * 
     * Thread-safe: can be called from any thread
     * 
     * Note: This searches through all subscribers (O(n)).
     * Keep a reference to the queue to unsubscribe later.
     */
    void unsubscribe(queue_ptr queue) {
        asio::post(strand_, [self = this->shared_from_this(), queue]() {
            // Search for the queue in subscribers
            for (auto it = self->subscribers_.begin(); it != self->subscribers_.end(); ++it) {
                if (it->second == queue) {
                    queue->stop();
                    self->subscribers_.erase(it);
                    self->subscriber_count_.fetch_sub(1, std::memory_order_relaxed);
                    return;
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
     * 
     * Thread-safe: This is the ONLY safe way to get subscriber count.
     * The synchronous version has been removed due to data race.
     */
    void get_subscriber_count(std::function<void(size_t)> callback) {
        asio::post(strand_, [self = this->shared_from_this(), callback = std::move(callback)]() {
            callback(self->subscribers_.size());
        });
    }

    /**
     * @brief Get approximate subscriber count (lockless)
     * 
     * WARNING: This uses atomic counter for lock-free read, but the value
     * may be stale. Use get_subscriber_count() if you need precise value.
     */
    size_t approx_subscriber_count() const noexcept {
        return subscriber_count_.load(std::memory_order_relaxed);
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
            self->subscriber_count_.store(0, std::memory_order_relaxed);
        });
    }

private:
    asio::strand<asio::io_context::executor_type> strand_;
    asio::io_context& io_context_;
    std::unordered_map<uint64_t, queue_ptr> subscribers_;  // 仅在 strand 内访问
    // next_id_ 使用 atomic：在 strand 外生成 ID，需要线程安全
    std::atomic<uint64_t> next_id_;
    // subscriber_count_ 使用 atomic：允许 approx_subscriber_count() 无锁读取近似值
    std::atomic<size_t> subscriber_count_{0};
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

} // namespace acore
