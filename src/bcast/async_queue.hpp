//
// Created by ubuntu on 10/10/25.
//

#pragma once

#include <asio.hpp>
#include <asio/experimental/awaitable_operators.hpp>
#include <asio/steady_timer.hpp>
#include <deque>
#include <optional>
#include <memory>
#include <iostream>
#include <chrono>

namespace bcast {

/**
 * @brief Asynchronous queue with coroutine support using ASIO strand
 * 
 * This queue uses strand to serialize all operations, making it thread-safe
 * without explicit locks. Supports both callback and coroutine interfaces.
 * 
 * @tparam T Message type
 */
template <typename T>
class async_queue : public std::enable_shared_from_this<async_queue<T>> {
public:
    /**
     * @brief Construct async queue
     * @param io_context ASIO io_context for async operations
     */
    explicit async_queue(asio::io_context& io_context)
        : io_context_(io_context)
        , strand_(asio::make_strand(io_context))
        , stopped_(false)
    {
    }

    /**
     * @brief Push message to queue
     * @param msg Message to enqueue
     * 
     * Thread-safe: can be called from any thread
     */
    void push(T msg) {
        asio::post(strand_, [self = this->shared_from_this(), msg = std::move(msg)]() mutable {
            if (self->stopped_) {
                return;
            }
            
            self->queue_.push_back(std::move(msg));
            
            // If there's a pending read operation, complete it
            if (self->pending_handler_) {
                T message = std::move(self->queue_.front());
                self->queue_.pop_front();
                
                auto handler = std::move(self->pending_handler_);
                self->pending_handler_ = nullptr;
                
                handler(std::error_code{}, std::move(message));
            }
        });
    }

    /**
     * @brief Asynchronously read one message from queue (coroutine interface)
     * @return awaitable<T> Message read from queue
     * 
     * Usage: auto msg = co_await queue->async_read_msg();
     */
    template<typename CompletionToken = asio::default_completion_token_t<typename asio::strand<asio::io_context::executor_type>::executor_type>>
    auto async_read_msg(CompletionToken&& token = asio::default_completion_token_t<typename asio::strand<asio::io_context::executor_type>::executor_type>{}) {
        return asio::async_initiate<CompletionToken, void(std::error_code, T)>(
            [self = this->shared_from_this()](auto handler) mutable {
                asio::post(self->strand_, [self, handler = std::move(handler)]() mutable {
                    if (self->stopped_) {
                        asio::post(asio::bind_executor(
                            self->strand_,
                            [handler = std::move(handler)]() mutable {
                                handler(asio::error::operation_aborted, T{});
                            }
                        ));
                        return;
                    }
                    
                    // If message available, return immediately
                    if (!self->queue_.empty()) {
                        T msg = std::move(self->queue_.front());
                        self->queue_.pop_front();
                        
                        asio::post(asio::bind_executor(
                            self->strand_,
                            [handler = std::move(handler), msg = std::move(msg)]() mutable {
                                handler(std::error_code{}, std::move(msg));
                            }
                        ));
                    } else {
                        // Queue empty, save handler for later
                        self->pending_handler_ = std::move(handler);
                    }
                });
            },
            token
        );
    }

    /**
     * @brief Asynchronously read multiple messages (up to max_count)
     * @param max_count Maximum number of messages to read
     * @return awaitable<std::vector<T>> Messages read from queue
     * 
     * Usage: auto msgs = co_await queue->async_read_msgs(10);
     */
    template<typename CompletionToken = asio::default_completion_token_t<typename asio::strand<asio::io_context::executor_type>::executor_type>>
    auto async_read_msgs(size_t max_count, CompletionToken&& token = asio::default_completion_token_t<typename asio::strand<asio::io_context::executor_type>::executor_type>{}) {
        return asio::async_initiate<CompletionToken, void(std::error_code, std::vector<T>)>(
            [self = this->shared_from_this(), max_count](auto handler) mutable {
                asio::post(self->strand_, [self, max_count, handler = std::move(handler)]() mutable {
                    if (self->stopped_) {
                        asio::post(asio::bind_executor(
                            self->strand_,
                            [handler = std::move(handler)]() mutable {
                                handler(asio::error::operation_aborted, std::vector<T>{});
                            }
                        ));
                        return;
                    }
                    
                    std::vector<T> messages;
                    size_t count = std::min(max_count, self->queue_.size());
                    messages.reserve(count);
                    
                    for (size_t i = 0; i < count; ++i) {
                        messages.push_back(std::move(self->queue_.front()));
                        self->queue_.pop_front();
                    }
                    
                    asio::post(asio::bind_executor(
                        self->strand_,
                        [handler = std::move(handler), messages = std::move(messages)]() mutable {
                            handler(std::error_code{}, std::move(messages));
                        }
                    ));
                });
            },
            token
        );
    }

    /**
     * @brief Asynchronously read one message with timeout
     * @param timeout Timeout duration
     * @return awaitable<T> Message read from queue
     * 
     * Usage: auto [ec, msg] = co_await queue->async_read_msg_with_timeout(5s, use_awaitable);
     * Returns asio::error::timed_out if timeout occurs.
     */
    template<typename Duration, typename CompletionToken = asio::default_completion_token_t<typename asio::strand<asio::io_context::executor_type>::executor_type>>
    auto async_read_msg_with_timeout(Duration timeout, CompletionToken&& token = asio::default_completion_token_t<typename asio::strand<asio::io_context::executor_type>::executor_type>{}) {
        return asio::async_initiate<CompletionToken, void(std::error_code, T)>(
            [self = this->shared_from_this(), timeout](auto handler) mutable {
                auto timer = std::make_shared<asio::steady_timer>(self->io_context_);
                timer->expires_after(timeout);
                
                // Timeout handler
                timer->async_wait([self, handler, timer](std::error_code ec) mutable {
                    if (ec == asio::error::operation_aborted) return;
                    
                    asio::post(self->strand_, [self, handler = std::move(handler)]() mutable {
                        if (self->pending_handler_) {
                            self->pending_handler_ = nullptr;
                            handler(asio::error::timed_out, T{});
                        }
                    });
                });
                
                // Try to read message immediately
                asio::post(self->strand_, [self, handler = std::move(handler), timer]() mutable {
                    if (self->stopped_) {
                        timer->cancel();
                        handler(asio::error::operation_aborted, T{});
                        return;
                    }
                    
                    if (!self->queue_.empty()) {
                        // Message available - return immediately
                        T msg = std::move(self->queue_.front());
                        self->queue_.pop_front();
                        timer->cancel();
                        handler(std::error_code{}, std::move(msg));
                    } else {
                        // Queue empty - wait for message or timeout
                        self->pending_handler_ = [handler = std::move(handler), timer](std::error_code ec, T msg) mutable {
                            timer->cancel();
                            handler(ec, std::move(msg));
                        };
                    }
                });
            },
            token
        );
    }

    /**
     * @brief Asynchronously read multiple messages with timeout
     * @param max_count Maximum number of messages to read
     * @param timeout Timeout duration
     * @return awaitable<std::vector<T>> Messages read from queue
     * 
     * Usage: auto [ec, msgs] = co_await queue->async_read_msgs_with_timeout(10, 5s, use_awaitable);
     * Returns asio::error::timed_out if timeout occurs and no messages are available.
     * If timeout occurs but some messages are available, returns those messages without error.
     */
    template<typename Duration, typename CompletionToken = asio::default_completion_token_t<typename asio::strand<asio::io_context::executor_type>::executor_type>>
    auto async_read_msgs_with_timeout(size_t max_count, Duration timeout, CompletionToken&& token = asio::default_completion_token_t<typename asio::strand<asio::io_context::executor_type>::executor_type>{}) {
        return asio::async_initiate<CompletionToken, void(std::error_code, std::vector<T>)>(
            [self = this->shared_from_this(), max_count, timeout](auto handler) mutable {
                auto timer = std::make_shared<asio::steady_timer>(self->io_context_);
                auto handler_ptr = std::make_shared<decltype(handler)>(std::move(handler));
                auto completed = std::make_shared<bool>(false);
                
                // Helper: read messages from queue
                auto read_messages = [self, max_count]() -> std::vector<T> {
                    std::vector<T> messages;
                    size_t count = std::min(max_count, self->queue_.size());
                    messages.reserve(count);
                    for (size_t i = 0; i < count; ++i) {
                        messages.push_back(std::move(self->queue_.front()));
                        self->queue_.pop_front();
                    }
                    return messages;
                };
                
                // Helper: complete operation once
                auto complete_once = [handler_ptr, completed](std::error_code ec, std::vector<T> msgs) {
                    if (!completed->exchange(true)) {
                        (*handler_ptr)(ec, std::move(msgs));
                    }
                };
                
                // Set up timeout
                timer->expires_after(timeout);
                timer->async_wait([self, complete_once, read_messages, timer](std::error_code ec) mutable {
                    if (ec == asio::error::operation_aborted) return;
                    
                    asio::post(self->strand_, [self, complete_once, read_messages]() mutable {
                        auto messages = read_messages();
                        auto result_ec = messages.empty() ? asio::error::timed_out : std::error_code{};
                        complete_once(result_ec, std::move(messages));
                    });
                });
                
                // Try to read immediately
                asio::post(self->strand_, [self, timer, complete_once, read_messages]() mutable {
                    if (self->stopped_) {
                        timer->cancel();
                        complete_once(asio::error::operation_aborted, std::vector<T>{});
                        return;
                    }
                    
                    if (!self->queue_.empty()) {
                        timer->cancel();
                        auto messages = read_messages();
                        complete_once(std::error_code{}, std::move(messages));
                    }
                    // Otherwise, wait for timeout
                });
            },
            token
        );
    }

    /**
     * @brief Stop the queue from processing more messages
     */
    void stop() {
        asio::post(strand_, [self = this->shared_from_this()]() {
            self->stopped_ = true;
            self->queue_.clear();
            
            // Cancel pending operations
            if (self->pending_handler_) {
                auto handler = std::move(self->pending_handler_);
                self->pending_handler_ = nullptr;
                handler(asio::error::operation_aborted, T{});
            }
        });
    }

    /**
     * @brief Check if queue is stopped
     */
    bool is_stopped() const {
        return stopped_;
    }

    /**
     * @brief Get current queue size (synchronous, use carefully)
     */
    size_t size() const {
        return queue_.size();
    }

private:
    asio::io_context& io_context_;
    asio::strand<asio::io_context::executor_type> strand_;
    std::deque<T> queue_;
    bool stopped_;
    
    // Pending async read handler
    std::function<void(std::error_code, T)> pending_handler_;
};

} // namespace bcast