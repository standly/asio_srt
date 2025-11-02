// srt_reactor.cpp

#include "srt_reactor.hpp"
#include "srt_error.hpp"
#include "srt_log.hpp"
#include <vector>
#include <iostream>
#include <future>
#include <atomic>

namespace asrt {

// ========================================
// 构造和析构
// ========================================

SrtReactor::SrtReactor(asio::io_context& io_context)
    : io_context_(io_context),
      reactor_strand_(asio::make_strand(io_context_)),
      srt_epoll_id_(SRT_ERROR) {
    
    initialize();
}

SrtReactor::~SrtReactor() {
    shutdown();
}

void SrtReactor::initialize() {
    // 初始化 SRT 和日志系统
    srt_startup();
    asrt::SrtLog::init(asrt::LogLevel::Notice);
    
    ASRT_LOG_INFO("SrtReactor initializing...");
    
    srt_epoll_id_ = srt_epoll_create();
    if (srt_epoll_id_ < 0) {
        const auto err_msg = std::string("Failed to create SRT epoll: ") + srt_getlasterror_str();
        ASRT_LOG_ERROR("{}", err_msg);
        throw std::runtime_error(err_msg);
    }
    
    ASRT_LOG_INFO("SRT epoll created (id={})", srt_epoll_id_);
    
    running_ = true;
    
    // ✅ 只启动轮询线程
    srt_poll_thread_ = std::thread(&SrtReactor::poll_loop, this);
    
    ASRT_LOG_INFO("SrtReactor started");
}

void SrtReactor::shutdown() {
    ASRT_LOG_INFO("SrtReactor shutting down...");
    
    if (running_.exchange(false)) {
        if (srt_poll_thread_.joinable()) {
            srt_poll_thread_.join();
            ASRT_LOG_DEBUG("Poll thread joined");
        }
        
        if (srt_epoll_id_ >= 0) {
            srt_epoll_release(srt_epoll_id_);
            ASRT_LOG_DEBUG("SRT epoll released");
        }
    }
    
    srt_cleanup();
    ASRT_LOG_INFO("SrtReactor shut down successfully");
}


// Public API implementations
asio::awaitable<int> SrtReactor::async_wait_readable(SRTSOCKET srt_sock) {
    // Using a C++17 structured binding to unpack the tuple into ec and triggered_events.
    auto [ec, triggered_events] = co_await asio::async_initiate<void(std::error_code, int)>(
        [this, srt_sock](auto&& handler) {
            // The initiation function remains the same.
            async_add_op(srt_sock, SRT_EPOLL_IN | SRT_EPOLL_ERR, std::forward<decltype(handler)>(handler));
        },
        // It's good practice to be explicit that we expect a tuple.
        asio::as_tuple(asio::use_awaitable)
    );

    // Now, we manually implement the "throw on error" logic.
    if (ec) {
        throw asio::system_error(ec);
    }

    // If there was no error, we co_return just the integer value.
    // This matches the function's return type of asio::awaitable<int>.
    co_return triggered_events;
}

asio::awaitable<int> SrtReactor::async_wait_writable(SRTSOCKET srt_sock) {
    auto [ec, triggered_events] = co_await asio::async_initiate<void(std::error_code, int)>(
        [this, srt_sock](auto&& handler) {
            async_add_op(srt_sock, SRT_EPOLL_OUT | SRT_EPOLL_ERR, std::forward<decltype(handler)>(handler));
        },
        asio::as_tuple(asio::use_awaitable)
    );

    if (ec) {
        throw asio::system_error(ec);
    }

    co_return triggered_events;
}

// Timeout versions - 直接实现，不调用无超时版本
asio::awaitable<int> SrtReactor::async_wait_readable(SRTSOCKET srt_sock, std::chrono::milliseconds timeout) {
    auto [ec, result] = co_await asio::async_initiate<void(std::error_code, int)>(
        [this, srt_sock, timeout](auto&& handler) {
            async_add_op(srt_sock, SRT_EPOLL_IN | SRT_EPOLL_ERR, timeout, 
                       std::forward<decltype(handler)>(handler));
        },
        asio::as_tuple(asio::use_awaitable)
    );
    
    if (ec) {
        throw asio::system_error(ec);
    }
    
    co_return result;
}

asio::awaitable<int> SrtReactor::async_wait_writable(SRTSOCKET srt_sock, std::chrono::milliseconds timeout) {
    auto [ec, result] = co_await asio::async_initiate<void(std::error_code, int)>(
        [this, srt_sock, timeout](auto&& handler) {
            async_add_op(srt_sock, SRT_EPOLL_OUT | SRT_EPOLL_ERR, timeout, 
                       std::forward<decltype(handler)>(handler));
        },
        asio::as_tuple(asio::use_awaitable)
    );
    
    if (ec) {
        throw asio::system_error(ec);
    }
    
    co_return result;
}

// Private helper to manage adding operations
// TEMPLATE IMPLEMENTATIONS ARE NOW IN THE HEADER FILE (.hpp)


void SrtReactor::poll_loop() {
    // 使用 srt_epoll_uwait 可以获取每个 socket 的精确事件标志
    std::vector<SRT_EPOLL_EVENT> events(100);
    
    // ✅ Poll 超时时间（建议与时间轮 tick 间隔一致以获得最佳性能）
    // 但即使提前返回也没问题，TimingWheel 内部会自动检查时间
    constexpr int POLL_TIMEOUT_MS = 100;

    while (running_) {
        // ✅ 阻塞式轮询（可能因事件提前返回）
        int n = srt_epoll_uwait(srt_epoll_id_, events.data(), events.size(), POLL_TIMEOUT_MS);

        // ========================================
        // 批量投递所有处理到 strand（事件 + 超时）
        // ========================================
        asio::post(reactor_strand_, [this, 
            events_copy = (n > 0 ? std::vector<SRT_EPOLL_EVENT>(events.begin(), events.begin() + n) 
                                 : std::vector<SRT_EPOLL_EVENT>())
        ]() {
            // 1. 先处理 epoll 触发的事件（优先级最高）
            if (!events_copy.empty()) {
                // 在 strand 上批量处理所有事件
                for (const auto& event : events_copy) {
                    SRTSOCKET sock = event.fd;
                    int flags = event.events;
                    
                    auto it = pending_ops_.find(sock);
                    if (it == pending_ops_.end()) {
                        continue; // Operation might have been cancelled
                    }

                    auto& op = it->second;
                    
                    // ========================================
                    // 优先处理错误事件
                    // ========================================
                    if (flags & SRT_EPOLL_ERR) {
                        // 获取详细的 SRT 错误信息
                        const char* error_msg = nullptr;
                        auto ec = asrt::make_srt_error_code(error_msg);
                        
                        // 记录错误日志
                        if(error_msg)
                            ASRT_LOG_ERROR("Socket {} error: {} ({}) [events=0x{:x}]", sock, ec.message(), error_msg, flags);
                        else
                            ASRT_LOG_ERROR("Socket {} error: {} [events=0x{:x}]", sock, ec.message(), flags);
                        
                        // 收集所有需要通知的 handler
                        std::vector<std::pair<
                            std::function<void(std::error_code, int)>,
                            asio::any_io_executor
                        >> handlers_to_notify;
                        
                        // 通知 read_handler（如果存在）
                        if (op->read_handler) {
                            auto ex = asio::get_associated_executor(
                                op->read_handler, 
                                io_context_.get_executor()
                            );
                            handlers_to_notify.emplace_back(
                                std::move(op->read_handler), 
                                ex
                            );
                            op->clear_handler(SRT_EPOLL_IN);
                        }
                        
                        // 通知 write_handler（如果存在）
                        if (op->write_handler) {
                            auto ex = asio::get_associated_executor(
                                op->write_handler, 
                                io_context_.get_executor()
                            );
                            handlers_to_notify.emplace_back(
                                std::move(op->write_handler), 
                                ex
                            );
                            op->clear_handler(SRT_EPOLL_OUT);
                        }
                        
                        // 从 epoll 中移除并清理
                        srt_epoll_remove_usock(srt_epoll_id_, sock);
                        pending_ops_.erase(it);
                        
                        // Remove from timing wheel (both read and write timers)
                        timing_wheel_.remove(make_timer_id(sock, false));
                        timing_wheel_.remove(make_timer_id(sock, true));
                        
                        ASRT_LOG_DEBUG("Socket {} removed from epoll after error", sock);
                        
                        // 在 strand 外异步通知所有 handler
                        for (auto& [handler, executor] : handlers_to_notify) {
                            asio::post(executor, [h = std::move(handler), ec, flags]() mutable {
                                h(ec, flags);
                            });
                        }
                        
                        continue; // 错误处理完成，继续下一个事件
                    }
                
                    // ========================================
                    // 处理正常的可读/可写事件
                    // ========================================
                    
                    std::vector<std::pair<
                        std::function<void(std::error_code, int)>,
                        asio::any_io_executor
                    >> handlers_to_notify;
                    
                    // 处理可读事件
                    if ((flags & SRT_EPOLL_IN) && op->read_handler) {
                        ASRT_LOG_DEBUG("Socket {} readable", sock);
                        auto ex = asio::get_associated_executor(
                            op->read_handler, 
                            io_context_.get_executor()
                        );
                        handlers_to_notify.emplace_back(
                            std::move(op->read_handler), 
                            ex
                        );
                        op->clear_handler(SRT_EPOLL_IN);
                        timing_wheel_.remove(make_timer_id(sock, false));
                    }
                    
                    // 处理可写事件
                    if ((flags & SRT_EPOLL_OUT) && op->write_handler) {
                        ASRT_LOG_DEBUG("Socket {} writable", sock);
                        auto ex = asio::get_associated_executor(
                            op->write_handler, 
                            io_context_.get_executor()
                        );
                        handlers_to_notify.emplace_back(
                            std::move(op->write_handler), 
                            ex
                        );
                        op->clear_handler(SRT_EPOLL_OUT);
                        timing_wheel_.remove(make_timer_id(sock, true));
                    }
                    
                    // 更新或清理 epoll 状态
                    if (op->is_empty()) {
                        srt_epoll_remove_usock(srt_epoll_id_, sock);
                        pending_ops_.erase(it);
                        timing_wheel_.remove(make_timer_id(sock, false));
                        timing_wheel_.remove(make_timer_id(sock, true));
                    } else {
                        int srt_events = op->events;
                        srt_epoll_update_usock(srt_epoll_id_, sock, &srt_events);
                    }
                    
                    // 异步通知所有 handler（成功，无错误）
                    for (auto& [handler, executor] : handlers_to_notify) {
                        asio::post(executor, [h = std::move(handler), flags]() mutable {
                            h({}, flags);  // 空 error_code = 成功
                        });
                    }
                } // end for each event
            }
            
            // 2. 处理超时（每次 poll 循环都调用 tick）
            // ✅ TimingWheel 内部会自动检查时间间隔，只有时间够了才真正 tick
            // 即使 epoll 提前返回（如 50ms），tick() 也会返回空列表
            auto expired_timer_ids = timing_wheel_.tick();
            for (uint64_t timer_id : expired_timer_ids) {
                SRTSOCKET sock = get_socket_from_timer_id(timer_id);
                bool is_write = is_write_timer(timer_id);
                int event_type = is_write ? SRT_EPOLL_OUT : SRT_EPOLL_IN;
                
                ASRT_LOG_DEBUG("Socket {} {} operation timed out", 
                             sock, is_write ? "write" : "read");
                
                // 直接调用（已经在 strand 上）
                cleanup_op(sock, event_type, std::make_error_code(std::errc::timed_out));
            }
        });
    }
}

void SrtReactor::cleanup_op(SRTSOCKET srt_sock, int event_type, const std::error_code& ec) {
    // Assumed to be called on the op_strand_
    auto it = pending_ops_.find(srt_sock);
    if (it == pending_ops_.end()) {
        return; // Already completed or cleaned up
    }
    
    if (ec) {
        ASRT_LOG_DEBUG("Cleaning up socket {} with error: {}", srt_sock, ec.message());
    } else {
        ASRT_LOG_DEBUG("Cleaning up cancelled operation for socket {}", srt_sock);
    }

    auto& op = it->second;
    std::function<void(std::error_code, int)> handler_to_call;

    if ((event_type & SRT_EPOLL_IN) && op->read_handler) {
        handler_to_call = std::move(op->read_handler);
        op->clear_handler(SRT_EPOLL_IN);
        
        // Remove read timeout
        timing_wheel_.remove(make_timer_id(srt_sock, false));
    } else if ((event_type & SRT_EPOLL_OUT) && op->write_handler) {
        handler_to_call = std::move(op->write_handler);
        op->clear_handler(SRT_EPOLL_OUT);
        
        // Remove write timeout
        timing_wheel_.remove(make_timer_id(srt_sock, true));
    }


    // After cancellation, update or remove the socket from epoll
    if (op->is_empty()) {
        srt_epoll_remove_usock(srt_epoll_id_, srt_sock);
        pending_ops_.erase(it);
        
        // Remove any remaining timers
        timing_wheel_.remove(make_timer_id(srt_sock, false));
        timing_wheel_.remove(make_timer_id(srt_sock, true));
    } else {
        int srt_events = op->events;
        srt_epoll_update_usock(srt_epoll_id_, srt_sock, &srt_events);
    }

    if (handler_to_call) {
        auto ex = asio::get_associated_executor(handler_to_call, io_context_.get_executor());
        asio::post(ex, [h = std::move(handler_to_call), ec]() mutable {
            h(ec, 0);
        });
    }
}
}