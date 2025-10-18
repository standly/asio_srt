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

    // 禁止拷贝和移动（设计上通过 shared_ptr 使用）
    dispatcher(const dispatcher&) = delete;
    dispatcher& operator=(const dispatcher&) = delete;
    dispatcher(dispatcher&&) = delete;
    dispatcher& operator=(dispatcher&&) = delete;

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
     * 
     * 返回队列立即可用，但订阅操作是异步的。
     * 
     * ⚠️ 重要：
     * - 队列立即返回，可以开始读取
     * - 但实际添加到 subscribers_ 是异步的（post 到 strand）
     * - 因此，订阅后立即 publish 的消息可能不会被新订阅者接收
     * 
     * 使用建议：
     * - 如果需要确保接收所有消息，订阅后稍等（使用异步API）
     * - 或者在订阅完成后再开始 publish
     * 
     * 示例（确保订阅完成）：
     * @code
     * auto queue = disp->subscribe();
     * // 等待订阅完成
     * size_t count = co_await disp->async_subscriber_count(use_awaitable);
     * // 现在可以安全地依赖接收消息
     * @endcode
     */
    queue_ptr subscribe() {
        auto queue = std::make_shared<async_queue<T>>(io_context_);
        uint64_t id = next_id_.fetch_add(1, std::memory_order_relaxed);
        
        asio::post(strand_, [self = this->shared_from_this(), id, queue]() {
            self->subscribers_[id] = queue;
        });
        
        return queue;
    }

    /**
     * @brief Unsubscribe by subscriber ID
     * @param subscriber_id The ID returned from subscribe_with_id()
     */
    void unsubscribe(uint64_t subscriber_id) {
        asio::post(strand_, [self = this->shared_from_this(), subscriber_id]() {
            auto it = self->subscribers_.find(subscriber_id);
            if (it != self->subscribers_.end()) {
                it->second->stop();
                self->subscribers_.erase(it);
            }
        });
    }
    
    /**
     * @brief Subscribe and get both queue and subscriber ID
     * @return pair of (subscriber_id, queue_ptr)
     */
    std::pair<uint64_t, queue_ptr> subscribe_with_id() {
        auto queue = std::make_shared<async_queue<T>>(io_context_);
        uint64_t id = next_id_.fetch_add(1, std::memory_order_relaxed);
        
        asio::post(strand_, [self = this->shared_from_this(), id, queue]() {
            self->subscribers_[id] = queue;
        });
        
        return {id, queue};
    }


    /**
     * @brief Publish message to all subscribers
     * @param msg Message to broadcast
     * 
     * 行为：消息被复制并异步推送给所有订阅者
     * - 如果队列已停止，消息会被静默丢弃（safe）
     * - 不会移除已停止的队列（需手动unsubscribe）
     * 
     * 性能注意事项：
     * - 消息会被复制给每个订阅者（对于小消息通常很快）
     * - 对于大消息（如大buffer），建议将消息类型定义为 shared_ptr<LargeData>
     * - 这样只会复制指针，避免深拷贝
     * - 两级异步分发（dispatcher strand + 各队列strand）
     * - 高吞吐场景建议使用批量操作
     */
    void publish(const T& msg) {
        asio::post(strand_, [self = this->shared_from_this(), msg]() {
            for (auto& [id, queue] : self->subscribers_) {
                // 推送到队列（已停止的队列会忽略消息）
                queue->push(msg);
            }
        });
    }

    /**
     * @brief Publish message to all subscribers (rvalue version)
     * @param msg Message to broadcast
     * 
     * Note: All subscribers get a copy. The rvalue is consumed internally.
     */
    void publish(T&& msg) {
        publish(static_cast<const T&>(msg));
    }

    /**
     * @brief Publish multiple messages (batch operation)
     * @param messages Vector of messages to broadcast
     * 
     * More efficient than calling publish() multiple times.
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
     */
    void publish_batch(std::initializer_list<T> init_list) {
        if (init_list.size() == 0) {
            return;
        }
        
        std::vector<T> messages(init_list);
        publish_batch(std::move(messages));
    }

    /**
     * @brief 获取订阅者数量
     */
    template<typename CompletionToken = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>>
    auto async_subscriber_count(CompletionToken&& token = asio::default_completion_token_t<asio::strand<asio::any_io_executor>>{}) {
        return asio::async_initiate<CompletionToken, void(size_t)>(
            [self = this->shared_from_this()](auto handler) {
                asio::post(self->strand_, [self, handler = std::move(handler)]() mutable {
                    std::move(handler)(self->subscribers_.size());
                });
            },
            token
        );
    }

    /**
     * @brief Clear all subscribers
     */
    void clear() {
        asio::post(strand_, [self = this->shared_from_this()]() {
            for (auto& [id, queue] : self->subscribers_) {
                queue->stop();
            }
            self->subscribers_.clear();
        });
    }

private:
    asio::strand<asio::io_context::executor_type> strand_;
    asio::io_context& io_context_;
    std::unordered_map<uint64_t, queue_ptr> subscribers_;  // 仅在 strand 内访问
    // next_id_ 使用 atomic：在 strand 外生成 ID，需要线程安全（正确使用）
    std::atomic<uint64_t> next_id_;
    // subscriber_count_ 移除：使用 async API 而不是无锁读取近似值
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
