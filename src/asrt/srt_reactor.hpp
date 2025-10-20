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

    // Helper to add an operation. This avoids duplicating code between async_wait_readable/writable.
    template<typename Handler>
    void async_add_op(SRTSOCKET srt_sock, int event_type, Handler&& handler);

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
};
}