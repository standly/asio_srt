// srt_reactor.h
#pragma once

#include <asio.hpp>
#include <asio/awaitable.hpp>
#include <asio/co_spawn.hpp>
#include <asio/strand.hpp>
#include <srt/srt.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>
#include <unordered_map>

#include "srt_log.hpp"
#include "srt_error.hpp"
#include "timing_wheel.hpp" // Include the new timing wheel header

namespace asrt {
class SrtReactor {
private:
    // Forward declaration
    struct EventOperation;

public:
    static SrtReactor& get_instance();

    SrtReactor(const SrtReactor&) = delete;
    SrtReactor& operator=(const SrtReactor&) = delete;

    // A clearer API: separate functions for read and write waits
    asio::awaitable<int> async_wait_readable(SRTSOCKET srt_sock);
    asio::awaitable<int> async_wait_writable(SRTSOCKET srt_sock);

    // Timeout versions - return -1 on timeout, or the event flags on success
    // Throws on other errors
    asio::awaitable<int> async_wait_readable(SRTSOCKET srt_sock, std::chrono::milliseconds timeout);
    asio::awaitable<int> async_wait_writable(SRTSOCKET srt_sock, std::chrono::milliseconds timeout);

    asio::io_context& get_io_context() {
        return io_context_;
    }
    
    // ========================================
    // 日志配置接口（统一使用 SRT 日志系统）
    // ========================================
    
    // 设置日志级别（同时影响 Reactor 和 SRT 协议栈）
    static void set_log_level(asrt::LogLevel level) {
        asrt::SrtLog::set_level(level);
    }
    
    // 获取当前日志级别
    static asrt::LogLevel get_log_level() {
        return asrt::SrtLog::get_level();
    }
    
    // 设置自定义日志回调
    // callback: 日志回调函数，签名为 void(LogLevel, const char* area, const char* message)
    //           传入 nullptr 表示恢复默认输出（stderr）
    // 注意：回调会接收 Reactor 和 SRT 库的所有日志
    static void set_log_callback(asrt::LogCallback callback) {
        asrt::SrtLog::set_callback(std::move(callback));
    }

private:
    SrtReactor();
    ~SrtReactor();

    void poll_loop();

    // Core implementation for adding an operation, must be called on op_strand_
    template<typename Handler>
    void add_op_on_strand(SRTSOCKET srt_sock, int event_type, Handler&& handler);

    // Public API to add an operation
    template<typename Handler>
    void async_add_op(SRTSOCKET srt_sock, int event_type, Handler&& handler);

    // Public API to add an operation with a timeout
    template<typename Handler>
    void async_add_op(SRTSOCKET srt_sock, int event_type, std::chrono::milliseconds timeout, Handler&& handler);


    // Cleanup now needs to know which event to cancel
    void cleanup_op(SRTSOCKET srt_sock, int event_type, const std::error_code& ec);

private:
    // The new EventOperation struct, as you proposed.
    struct EventOperation {
        std::function<void(std::error_code, int)> read_handler;
        std::function<void(std::error_code, int)> write_handler;
        uint32_t events = 0; // Combined events for srt_epoll

        [[nodiscard]] bool is_empty() const {
            return !read_handler && !write_handler;
        }

        // Helper to add a handler and update the event mask
        template <typename Handler>
        void add_handler(int event_type, Handler&& h) {
            auto hp = std::make_shared<std::decay_t<Handler>>(std::move(h));
            auto generic_handler = [hp](std::error_code ec, int result) {
                (*hp)(ec, result);
            };

            if (event_type & SRT_EPOLL_IN) {
                read_handler = generic_handler;
                events |= SRT_EPOLL_IN;
            }
            if (event_type & SRT_EPOLL_OUT) {
                write_handler = generic_handler;
                events |= SRT_EPOLL_OUT;
            }
        }

        // Helper to remove a handler and update the event mask
        void clear_handler(int event_type) {
            if (event_type & SRT_EPOLL_IN) {
                read_handler = nullptr;
                events &= ~SRT_EPOLL_IN;
            }
            if (event_type & SRT_EPOLL_OUT) {
                write_handler = nullptr;
                events &= ~SRT_EPOLL_OUT;
            }
        }
    };

private:
    asio::io_context io_context_;
    asio::executor_work_guard<asio::io_context::executor_type> work_guard_;
    asio::strand<asio::io_context::executor_type> op_strand_;

    std::thread asio_thread_;
    std::thread srt_poll_thread_;

    int srt_epoll_id_;
    std::atomic<bool> running_{false};

    // The map now holds EventOperations
    std::unordered_map<SRTSOCKET, std::unique_ptr<EventOperation>> pending_ops_;
    
    // ✅ Atomic counter for pending operations (for performance optimization)
    // Tracks the number of sockets in pending_ops_ to avoid expensive checks in poll_loop
    std::atomic<size_t> pending_ops_count_{0};

    // Timing wheel for managing timeouts efficiently
    // Key = (SRTSOCKET << 1) | event_flag, where event_flag: 0=READ, 1=WRITE
    // This allows separate timeouts for read and write operations
    TimingWheel<uint64_t> timing_wheel_;
    std::chrono::steady_clock::time_point last_tick_time_;
    
    // Helper functions to encode/decode timer IDs
    static constexpr uint64_t make_timer_id(SRTSOCKET sock, bool is_write) {
        return (static_cast<uint64_t>(sock) << 1) | (is_write ? 1 : 0);
    }
    static constexpr SRTSOCKET get_socket_from_timer_id(uint64_t timer_id) {
        return static_cast<SRTSOCKET>(timer_id >> 1);
    }
    static constexpr bool is_write_timer(uint64_t timer_id) {
        return (timer_id & 1) != 0;
    }
};


//==============================================================================
// Template Function Implementations
//==============================================================================

template<typename Handler>
void SrtReactor::async_add_op(SRTSOCKET srt_sock, int event_type, Handler&& handler) {
    asio::post(op_strand_, [this, srt_sock, event_type, h = std::move(handler)]() mutable {
        add_op_on_strand(srt_sock, event_type, std::move(h));
    });
}

template<typename Handler>
void SrtReactor::async_add_op(SRTSOCKET srt_sock, int event_type, std::chrono::milliseconds timeout, Handler&& handler) {
    asio::post(op_strand_, [this, srt_sock, event_type, timeout, h = std::move(handler)]() mutable {
        // Add to the timing wheel with separate timers for read and write
        if (event_type & SRT_EPOLL_IN) {
            timing_wheel_.add(make_timer_id(srt_sock, false), timeout);
        }
        if (event_type & SRT_EPOLL_OUT) {
            timing_wheel_.add(make_timer_id(srt_sock, true), timeout);
        }
        
        // Add the operation to epoll
        add_op_on_strand(srt_sock, event_type, std::move(h));
    });
}

template<typename Handler>
void SrtReactor::add_op_on_strand(SRTSOCKET srt_sock, int event_type, Handler&& handler) {
    // This is the core logic, now guaranteed to run on the strand.
    // Attach cancellation slot
    auto slot = asio::get_associated_cancellation_slot(handler);
    if (slot.is_connected()) {
        slot.assign([this, srt_sock, event_type](asio::cancellation_type type) {
            if (type != asio::cancellation_type::none) {
                // Post cancellation to the strand (safe, as it might be called from any thread)
                asio::post(op_strand_, [this, srt_sock, event_type]() {
                    cleanup_op(srt_sock, event_type, asio::error::operation_aborted);
                });
            }
        });
    }

    // Find or create the operation
    auto it = pending_ops_.find(srt_sock);
    if (it == pending_ops_.end()) {
        // Socket not found, prepare a new operation.
        auto op = std::make_unique<EventOperation>();
        int srt_events = 0;
        if (event_type & SRT_EPOLL_IN) srt_events |= SRT_EPOLL_IN;
        if (event_type & SRT_EPOLL_OUT) srt_events |= SRT_EPOLL_OUT;

        if (srt_epoll_add_usock(srt_epoll_id_, srt_sock, &srt_events) != 0) {
            const auto err_msg = std::string("Failed to add socket to epoll: ") + srt_getlasterror_str();
            ASRT_LOG_ERROR("{}", err_msg);
            auto ex = asio::get_associated_executor(handler);
            asio::post(ex, [h = std::move(handler)]() mutable {
                h(asrt::make_error_code(asrt::srt_errc::epoll_add_failed), 0);
            });
            return;
        }
        
        // Now that epoll succeeded, add the handler and store the operation.
        op->add_handler(event_type, std::move(handler));
        ASRT_LOG_DEBUG("Socket {} added to epoll (events=0x{:x})", srt_sock, srt_events);
        pending_ops_[srt_sock] = std::move(op);
        
        pending_ops_count_.fetch_add(1, std::memory_order_release);
    } else {
        // Socket found, prepare to update it.
        auto& op = it->second;
        int srt_events = op->events;
        if (event_type & SRT_EPOLL_IN) srt_events |= SRT_EPOLL_IN;
        if (event_type & SRT_EPOLL_OUT) srt_events |= SRT_EPOLL_OUT;

        if (srt_epoll_update_usock(srt_epoll_id_, srt_sock, &srt_events) != 0) {
             const auto err_msg = std::string("Failed to update socket in epoll: ") + srt_getlasterror_str();
             ASRT_LOG_ERROR("{}", err_msg);
             auto ex = asio::get_associated_executor(handler);
             asio::post(ex, [h = std::move(handler)]() mutable {
                h(asrt::make_error_code(asrt::srt_errc::epoll_update_failed), 0);
            });
            return;
        }
        
        // Now that epoll succeeded, add the handler.
        op->add_handler(event_type, std::move(handler));
        ASRT_LOG_DEBUG("Socket {} updated in epoll (events=0x{:x})", srt_sock, srt_events);
    }
}


} // namespace asrt