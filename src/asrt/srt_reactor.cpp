// srt_reactor.cpp

#include "srt_reactor.hpp"
#include "srt_error.hpp"
#include "srt_log.hpp"
#include <vector>
#include <iostream>
#include <future>
#include <atomic>

namespace asrt {
// Constructor, Destructor, and get_instance remain largely the same.
// ... (omitting them for brevity, they are unchanged from your original code)
SrtReactor& SrtReactor::get_instance() {
    static SrtReactor instance;
    return instance;
}

SrtReactor::SrtReactor()
    : work_guard_(asio::make_work_guard(io_context_)),
      op_strand_(asio::make_strand(io_context_)),
      srt_epoll_id_(SRT_ERROR) {

    // 初始化 SRT 和日志系统
    srt_startup();
    asrt::SrtLog::init(asrt::LogLevel::Notice);  // 默认 Notice 级别
    
    ASRT_LOG_INFO("SrtReactor initializing...");
    
    srt_epoll_id_ = srt_epoll_create();
    if (srt_epoll_id_ < 0) {
        const auto err_msg = std::string("Failed to create SRT epoll: ") + srt_getlasterror_str();
        ASRT_LOG_ERROR("{}", err_msg);
        throw std::runtime_error(err_msg);
    }
    
    ASRT_LOG_INFO("SRT epoll created (id={})", srt_epoll_id_);
    
    running_ = true;
    asio_thread_ = std::thread([this]() { io_context_.run(); });
    srt_poll_thread_ = std::thread(&SrtReactor::poll_loop, this);
    
    ASRT_LOG_INFO("SrtReactor started");
}

SrtReactor::~SrtReactor() {
    ASRT_LOG_INFO("SrtReactor shutting down...");
    
    if (running_.exchange(false)) {
        if (srt_epoll_id_ >= 0) {
            srt_epoll_release(srt_epoll_id_);
            ASRT_LOG_DEBUG("SRT epoll released");
        }
        if (srt_poll_thread_.joinable()) {
            srt_poll_thread_.join();
            ASRT_LOG_DEBUG("Poll thread joined");
        }
        work_guard_.reset();
        io_context_.stop();
        if (asio_thread_.joinable()) {
            asio_thread_.join();
            ASRT_LOG_DEBUG("ASIO thread joined");
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
    // 创建共享的状态变量（需要在多个协程间共享）
    auto timer = std::make_shared<asio::steady_timer>(io_context_);
    auto timed_out = std::make_shared<std::atomic<bool>>(false);
    auto cancel_signal = std::make_shared<asio::cancellation_signal>();
    
    timer->expires_after(timeout);
    
    // 启动定时器协程
    asio::co_spawn(
        io_context_,
        [timer, cancel_signal, timed_out]() -> asio::awaitable<void> {
            auto [ec] = co_await timer->async_wait(asio::as_tuple(asio::use_awaitable));
            if (!ec) {
                // 定时器正常到期（未被取消）= 超时
                timed_out->store(true);
                cancel_signal->emit(asio::cancellation_type::all);
            }
        },
        asio::detached
    );

    // 直接等待 socket 操作（带取消支持）
    auto [ec, result] = co_await asio::async_initiate<void(std::error_code, int)>(
        [this, srt_sock](auto&& handler) {
            async_add_op(srt_sock, SRT_EPOLL_IN | SRT_EPOLL_ERR, 
                       std::forward<decltype(handler)>(handler));
        },
        asio::as_tuple(asio::bind_cancellation_slot(cancel_signal->slot(), asio::use_awaitable))
    );
    
    // 取消定时器（如果还在运行）
    timer->cancel();
    
    // 处理结果
    if (ec) {
        if (ec == asio::error::operation_aborted && timed_out->load()) {
            // 超时使用标准的 timed_out 错误
            throw asio::system_error(std::make_error_code(std::errc::timed_out));
        }
        throw asio::system_error(ec); // 其他错误
    }
    
    co_return result; // 成功
}

asio::awaitable<int> SrtReactor::async_wait_writable(SRTSOCKET srt_sock, std::chrono::milliseconds timeout) {
    // 创建共享的状态变量
    auto timer = std::make_shared<asio::steady_timer>(io_context_);
    auto timed_out = std::make_shared<std::atomic<bool>>(false);
    auto cancel_signal = std::make_shared<asio::cancellation_signal>();
    
    timer->expires_after(timeout);
    
    // 启动定时器协程
    asio::co_spawn(
        io_context_,
        [timer, cancel_signal, timed_out]() -> asio::awaitable<void> {
            auto [ec] = co_await timer->async_wait(asio::as_tuple(asio::use_awaitable));
            if (!ec) {
                // 定时器正常到期（未被取消）= 超时
                timed_out->store(true);
                cancel_signal->emit(asio::cancellation_type::all);
            }
        },
        asio::detached
    );

    // 直接等待 socket 操作（带取消支持）
    auto [ec, result] = co_await asio::async_initiate<void(std::error_code, int)>(
        [this, srt_sock](auto&& handler) {
            async_add_op(srt_sock, SRT_EPOLL_OUT | SRT_EPOLL_ERR, 
                       std::forward<decltype(handler)>(handler));
        },
        asio::as_tuple(asio::bind_cancellation_slot(cancel_signal->slot(), asio::use_awaitable))
    );
    
    // 取消定时器（如果还在运行）
    timer->cancel();
    
    // 处理结果
    if (ec) {
        if (ec == asio::error::operation_aborted && timed_out->load()) {
            // 超时使用标准的 timed_out 错误
            throw asio::system_error(std::make_error_code(std::errc::timed_out));
        }
        throw asio::system_error(ec); // 其他错误
    }
    
    co_return result; // 成功
}

// Private helper to manage adding operations
template<typename Handler>
void SrtReactor::async_add_op(SRTSOCKET srt_sock, int event_type, Handler&& handler) {
    auto slot = asio::get_associated_cancellation_slot(handler);
    if (slot.is_connected()) {
        slot.assign([this, srt_sock, event_type](asio::cancellation_type type) {
            if (type != asio::cancellation_type::none) {
                // Post cancellation to the strand
                asio::post(op_strand_, [this, srt_sock, event_type]() {
                    cleanup_op(srt_sock, event_type, asio::error::operation_aborted);
                });
            }
        });
    }

    asio::post(op_strand_, [this, srt_sock, event_type, h = std::move(handler)]() mutable {
        auto it = pending_ops_.find(srt_sock);
        if (it == pending_ops_.end()) {
            // Socket not found, create a new operation and add to epoll
            auto op = std::make_unique<EventOperation>();
            op->add_handler(event_type, std::move(h));

            int srt_events = op->events;
            if (srt_epoll_add_usock(srt_epoll_id_, srt_sock, &srt_events) != 0) {
                const auto err_msg = std::string("Failed to add socket to epoll: ") + srt_getlasterror_str();
                ASRT_LOG_ERROR("{}", err_msg);
                auto ex = asio::get_associated_executor(h);
                asio::post(ex, [h_moved = std::move(h)]() mutable {
                    h_moved(asrt::make_error_code(asrt::srt_errc::epoll_add_failed), 0);
                });
                return;
            }
            ASRT_LOG_DEBUG("Socket {} added to epoll (events=0x{:x})", srt_sock, srt_events);
            pending_ops_[srt_sock] = std::move(op);
        } else {
            // Socket found, update existing operation and modify epoll
            auto& op = it->second;
            op->add_handler(event_type, std::move(h));

            int srt_events = op->events;
            if (srt_epoll_update_usock(srt_epoll_id_, srt_sock, &srt_events) != 0) {
                 const auto err_msg = std::string("Failed to update socket in epoll: ") + srt_getlasterror_str();
                 ASRT_LOG_ERROR("{}", err_msg);
                 auto ex = asio::get_associated_executor(h);
                 asio::post(ex, [h_moved = std::move(h)]() mutable {
                    h_moved(asrt::make_error_code(asrt::srt_errc::epoll_update_failed), 0);
                });
                // Since we failed, roll back the handler addition
                op->clear_handler(event_type);
                return;
            }
            ASRT_LOG_DEBUG("Socket {} updated in epoll (events=0x{:x})", srt_sock, srt_events);
        }
    });
}

void SrtReactor::poll_loop() {
    // 使用 srt_epoll_uwait 可以获取每个 socket 的精确事件标志
    std::vector<SRT_EPOLL_EVENT> events(100);

    while (running_) {
        // Check if there are any pending operations using dispatch for synchronous execution
        // 如果没有 pending_ops 那么说明没有绑定要操作的socket到epoll，直接跳过
        std::atomic<bool> has_pending{false};
        std::promise<void> check_done;
        auto check_future = check_done.get_future();
        
        asio::post(op_strand_, [this, &has_pending, &check_done]() {
            has_pending = !pending_ops_.empty();
            check_done.set_value();
        });
        
        check_future.wait();
        
        if (!has_pending) {
            // Sleep briefly to avoid busy waiting
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        // 使用 srt_epoll_uwait 获取详细的事件信息
        // Timeout of 100ms
        int n = srt_epoll_uwait(srt_epoll_id_, events.data(), events.size(), 100);

        if (n <= 0) continue;

        // 处理每个触发的 socket 事件
        for (int i = 0; i < n; i++) {
            SRTSOCKET sock = events[i].fd;
            int flags = events[i].events;

            asio::post(op_strand_, [this, sock, flags]() {
                auto it = pending_ops_.find(sock);
                if (it == pending_ops_.end()) {
                    return; // Operation might have been cancelled
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
                    
                    ASRT_LOG_DEBUG("Socket {} removed from epoll after error", sock);
                    
                    // 在 strand 外异步通知所有 handler
                    for (auto& [handler, executor] : handlers_to_notify) {
                        asio::post(executor, [h = std::move(handler), ec, flags]() mutable {
                            h(ec, flags);
                        });
                    }
                    
                    return; // 错误处理完成，不再处理其他事件
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
                }
                
                // 更新或清理 epoll 状态
                if (op->is_empty()) {
                    srt_epoll_remove_usock(srt_epoll_id_, sock);
                    pending_ops_.erase(it);
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
            });
        }
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
    } else if ((event_type & SRT_EPOLL_OUT) && op->write_handler) {
        handler_to_call = std::move(op->write_handler);
        op->clear_handler(SRT_EPOLL_OUT);
    }


    // After cancellation, update or remove the socket from epoll
    if (op->is_empty()) {
        srt_epoll_remove_usock(srt_epoll_id_, srt_sock);
        pending_ops_.erase(it);
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