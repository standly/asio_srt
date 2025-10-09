// test_srt_reactor.cpp
#include <gtest/gtest.h>
#include "asrt/srt_reactor.hpp"
#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/steady_timer.hpp>
#include <chrono>
#include <thread>

using namespace std::chrono_literals;

class SrtReactorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // SrtReactor is a singleton, so it's already initialized
        reactor_ = &SrtReactor::get_instance();
    }

    void TearDown() override {
        // Clean up any sockets created during tests
        for (auto sock : test_sockets_) {
            if (sock != SRT_INVALID_SOCK) {
                srt_close(sock);
            }
        }
        test_sockets_.clear();
    }

    // Helper to create a pair of connected SRT sockets
    std::pair<SRTSOCKET, SRTSOCKET> create_socket_pair() {
        SRTSOCKET listener = srt_create_socket();
        if (listener == SRT_INVALID_SOCK) {
            ADD_FAILURE() << "Failed to create listener socket";
            return {SRT_INVALID_SOCK, SRT_INVALID_SOCK};
        }
        test_sockets_.push_back(listener);

        // Set blocking mode for connection setup for reliability
        int yes = 1;
        int no = 0;
        srt_setsockopt(listener, 0, SRTO_RCVSYN, &yes, sizeof(yes));
        srt_setsockopt(listener, 0, SRTO_SNDSYN, &yes, sizeof(yes));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = 0; // Let OS assign port

        int bind_result = srt_bind(listener, (sockaddr*)&addr, sizeof(addr));
        if (bind_result != 0) {
            ADD_FAILURE() << "Failed to bind: " << srt_getlasterror_str();
            return {SRT_INVALID_SOCK, SRT_INVALID_SOCK};
        }

        // Get the actual bound address
        int addr_len = sizeof(addr);
        srt_getsockname(listener, (sockaddr*)&addr, &addr_len);

        int listen_result = srt_listen(listener, 1);
        if (listen_result != 0) {
            ADD_FAILURE() << "Failed to listen: " << srt_getlasterror_str();
            return {SRT_INVALID_SOCK, SRT_INVALID_SOCK};
        }

        // Create client socket
        SRTSOCKET client = srt_create_socket();
        if (client == SRT_INVALID_SOCK) {
            ADD_FAILURE() << "Failed to create client socket";
            return {SRT_INVALID_SOCK, SRT_INVALID_SOCK};
        }
        test_sockets_.push_back(client);

        // Connect in a separate thread to avoid blocking
        std::thread connect_thread([client, &addr]() {
            srt_connect(client, (sockaddr*)&addr, sizeof(addr));
        });

        // Accept the connection
        sockaddr_storage their_addr{};
        int their_addr_len = sizeof(their_addr);
        
        SRTSOCKET server = srt_accept(listener, (sockaddr*)&their_addr, &their_addr_len);
        
        // Wait for connect to complete
        connect_thread.join();
        
        if (server == SRT_INVALID_SOCK) {
            ADD_FAILURE() << "Failed to accept connection: " << srt_getlasterror_str();
            return {SRT_INVALID_SOCK, SRT_INVALID_SOCK};
        }
        
        test_sockets_.push_back(server);
        
        // Now set non-blocking mode for actual testing
        srt_setsockopt(client, 0, SRTO_RCVSYN, &no, sizeof(no));
        srt_setsockopt(client, 0, SRTO_SNDSYN, &no, sizeof(no));
        srt_setsockopt(server, 0, SRTO_RCVSYN, &no, sizeof(no));
        srt_setsockopt(server, 0, SRTO_SNDSYN, &no, sizeof(no));

        // Close listener as we don't need it anymore
        srt_close(listener);
        test_sockets_.erase(std::remove(test_sockets_.begin(), test_sockets_.end(), listener), 
                           test_sockets_.end());

        return {client, server};
    }

    SrtReactor* reactor_;
    std::vector<SRTSOCKET> test_sockets_;
};

// Test 1: Basic singleton access
TEST_F(SrtReactorTest, SingletonAccess) {
    auto& reactor1 = SrtReactor::get_instance();
    auto& reactor2 = SrtReactor::get_instance();
    EXPECT_EQ(&reactor1, &reactor2);
}

// Test 2: IO Context availability
TEST_F(SrtReactorTest, IoContextAvailable) {
    auto& io_ctx = reactor_->get_io_context();
    EXPECT_FALSE(io_ctx.stopped());
}

// Test 3: Socket writable after creation
TEST_F(SrtReactorTest, SocketWritableAfterCreation) {
    auto [client, server] = create_socket_pair();
    if (client == SRT_INVALID_SOCK || server == SRT_INVALID_SOCK) {
        GTEST_SKIP() << "Failed to create socket pair";
    }

    bool test_completed = false;
    std::exception_ptr test_exception;

    asio::co_spawn(
        reactor_->get_io_context(),
        [&]() -> asio::awaitable<void> {
            try {
                // Client socket should be writable
                int events = co_await reactor_->async_wait_writable(client);
                EXPECT_NE(events & SRT_EPOLL_OUT, 0);
                test_completed = true;
            } catch (...) {
                test_exception = std::current_exception();
            }
        },
        asio::detached
    );

    // Wait for test to complete (with timeout)
    auto start = std::chrono::steady_clock::now();
    while (!test_completed && 
           std::chrono::steady_clock::now() - start < 5s) {
        std::this_thread::sleep_for(10ms);
    }

    if (test_exception) {
        std::rethrow_exception(test_exception);
    }

    EXPECT_TRUE(test_completed) << "Test did not complete in time";
}

// Test 4: Send and receive data
TEST_F(SrtReactorTest, SendReceiveData) {
    auto [client, server] = create_socket_pair();
    if (client == SRT_INVALID_SOCK || server == SRT_INVALID_SOCK) {
        GTEST_SKIP() << "Failed to create socket pair";
    }

    bool send_completed = false;
    bool recv_completed = false;
    std::exception_ptr test_exception;

    const char* test_message = "Hello, SRT!";
    const size_t msg_len = strlen(test_message);
    char recv_buffer[1500] = {0};  // Use larger buffer to avoid SRT warning

    // Receiver coroutine - start first to be ready
    asio::co_spawn(
        reactor_->get_io_context(),
        [&]() -> asio::awaitable<void> {
            try {
                co_await reactor_->async_wait_readable(server);
                int received = srt_recv(server, recv_buffer, sizeof(recv_buffer));
                EXPECT_GT(received, 0) << "Failed to receive data: " << srt_getlasterror_str();
                if (received > 0) {
                    recv_buffer[received] = '\0';  // Null terminate
                    EXPECT_STREQ(recv_buffer, test_message);
                }
                recv_completed = true;
            } catch (...) {
                test_exception = std::current_exception();
            }
        },
        asio::detached
    );

    // Small delay to ensure receiver is waiting
    std::this_thread::sleep_for(50ms);

    // Sender coroutine
    asio::co_spawn(
        reactor_->get_io_context(),
        [&]() -> asio::awaitable<void> {
            try {
                co_await reactor_->async_wait_writable(client);
                int sent = srt_send(client, test_message, msg_len);
                EXPECT_GT(sent, 0) << "Failed to send data: " << srt_getlasterror_str();
                send_completed = true;
            } catch (...) {
                test_exception = std::current_exception();
            }
        },
        asio::detached
    );

    // Wait for both operations to complete
    auto start = std::chrono::steady_clock::now();
    while ((!send_completed || !recv_completed) && 
           std::chrono::steady_clock::now() - start < 5s) {
        std::this_thread::sleep_for(10ms);
    }

    if (test_exception) {
        std::rethrow_exception(test_exception);
    }

    EXPECT_TRUE(send_completed) << "Send did not complete in time";
    EXPECT_TRUE(recv_completed) << "Receive did not complete in time";
}

// Test 5: Multiple concurrent operations
TEST_F(SrtReactorTest, MultipleConcurrentOperations) {
    auto [client1, server1] = create_socket_pair();
    auto [client2, server2] = create_socket_pair();
    
    if (client1 == SRT_INVALID_SOCK || server1 == SRT_INVALID_SOCK ||
        client2 == SRT_INVALID_SOCK || server2 == SRT_INVALID_SOCK) {
        GTEST_SKIP() << "Failed to create socket pairs";
    }

    std::atomic<int> completed_ops{0};
    std::exception_ptr test_exception;

    // Launch 4 concurrent operations
    for (auto sock : {client1, server1, client2, server2}) {
        asio::co_spawn(
            reactor_->get_io_context(),
            [&, sock]() -> asio::awaitable<void> {
                try {
                    co_await reactor_->async_wait_writable(sock);
                    completed_ops++;
                } catch (...) {
                    test_exception = std::current_exception();
                }
            },
            asio::detached
        );
    }

    // Wait for all operations to complete
    auto start = std::chrono::steady_clock::now();
    while (completed_ops < 4 && 
           std::chrono::steady_clock::now() - start < 5s) {
        std::this_thread::sleep_for(10ms);
    }

    if (test_exception) {
        std::rethrow_exception(test_exception);
    }

    EXPECT_EQ(completed_ops, 4) << "Not all concurrent operations completed";
}

// Test 6: Operation cancellation
TEST_F(SrtReactorTest, OperationCancellation) {
    SRTSOCKET sock = srt_create_socket();
    ASSERT_NE(sock, SRT_INVALID_SOCK);
    test_sockets_.push_back(sock);

    // Set non-blocking
    int no = 0;
    srt_setsockopt(sock, 0, SRTO_RCVSYN, &no, sizeof(no));

    bool operation_cancelled = false;
    std::exception_ptr test_exception;

    asio::cancellation_signal cancel_signal;

    asio::co_spawn(
        reactor_->get_io_context(),
        [&]() -> asio::awaitable<void> {
            try {
                // Wait for readable on a socket that won't become readable
                co_await reactor_->async_wait_readable(sock);
                // Should not reach here - operation should have been cancelled
                ADD_FAILURE() << "Operation was not cancelled";
            } catch (const asio::system_error& e) {
                if (e.code() == asio::error::operation_aborted) {
                    operation_cancelled = true;
                } else {
                    test_exception = std::current_exception();
                }
            } catch (...) {
                test_exception = std::current_exception();
            }
        },
        asio::bind_cancellation_slot(cancel_signal.slot(), asio::detached)
    );

    // Wait a bit, then cancel
    std::this_thread::sleep_for(100ms);
    cancel_signal.emit(asio::cancellation_type::all);

    // Wait for cancellation to process
    auto start = std::chrono::steady_clock::now();
    while (!operation_cancelled && 
           std::chrono::steady_clock::now() - start < 2s) {
        std::this_thread::sleep_for(10ms);
    }

    if (test_exception) {
        std::rethrow_exception(test_exception);
    }

    EXPECT_TRUE(operation_cancelled) << "Operation was not cancelled properly";
}

// Test 7: Read and write on same socket
TEST_F(SrtReactorTest, SimultaneousReadWriteOperations) {
    auto [client, server] = create_socket_pair();
    if (client == SRT_INVALID_SOCK || server == SRT_INVALID_SOCK) {
        GTEST_SKIP() << "Failed to create socket pair";
    }

    bool write_completed = false;
    bool read_completed = false;
    std::exception_ptr test_exception;
    const char* msg = "test";
    char buffer[1500] = {0};

    // Start read operation first
    asio::co_spawn(
        reactor_->get_io_context(),
        [&]() -> asio::awaitable<void> {
            try {
                // Wait for client to be readable
                co_await reactor_->async_wait_readable(client);
                int received = srt_recv(client, buffer, sizeof(buffer));
                EXPECT_GT(received, 0);
                read_completed = true;
            } catch (...) {
                test_exception = std::current_exception();
            }
        },
        asio::detached
    );

    // Small delay to ensure read operation is registered
    std::this_thread::sleep_for(50ms);

    // Now start write operations
    asio::co_spawn(
        reactor_->get_io_context(),
        [&]() -> asio::awaitable<void> {
            try {
                // Client should be writable immediately
                co_await reactor_->async_wait_writable(client);
                write_completed = true;
            } catch (...) {
                test_exception = std::current_exception();
            }
        },
        asio::detached
    );

    asio::co_spawn(
        reactor_->get_io_context(),
        [&]() -> asio::awaitable<void> {
            try {
                // Write data from server to client
                co_await reactor_->async_wait_writable(server);
                int sent = srt_send(server, msg, strlen(msg));
                EXPECT_GT(sent, 0);
            } catch (...) {
                test_exception = std::current_exception();
            }
        },
        asio::detached
    );

    // Wait for both operations
    auto start = std::chrono::steady_clock::now();
    while ((!write_completed || !read_completed) && 
           std::chrono::steady_clock::now() - start < 5s) {
        std::this_thread::sleep_for(10ms);
    }

    if (test_exception) {
        std::rethrow_exception(test_exception);
    }

    EXPECT_TRUE(write_completed) << "Write operation did not complete";
    EXPECT_TRUE(read_completed) << "Read operation did not complete";
}

// Test 8: Socket cleanup after operations
TEST_F(SrtReactorTest, SocketCleanupAfterOperations) {
    auto [client, server] = create_socket_pair();
    if (client == SRT_INVALID_SOCK || server == SRT_INVALID_SOCK) {
        GTEST_SKIP() << "Failed to create socket pair";
    }

    bool operation_completed = false;
    std::exception_ptr test_exception;

    // Test that operations complete successfully
    asio::co_spawn(
        reactor_->get_io_context(),
        [&]() -> asio::awaitable<void> {
            try {
                // Wait for writable - should complete quickly
                co_await reactor_->async_wait_writable(client);
                operation_completed = true;
            } catch (...) {
                test_exception = std::current_exception();
            }
        },
        asio::detached
    );

    // Wait for operation to complete
    auto start = std::chrono::steady_clock::now();
    while (!operation_completed && 
           std::chrono::steady_clock::now() - start < 2s) {
        std::this_thread::sleep_for(10ms);
    }

    if (test_exception) {
        std::rethrow_exception(test_exception);
    }

    EXPECT_TRUE(operation_completed) << "Operation did not complete";
    
    // Now close the sockets and verify cleanup
    // The TearDown will handle closing, this just verifies no crash
}

// Test 9: Timeout on readable - should timeout
TEST_F(SrtReactorTest, TimeoutOnReadable) {
    auto [client, server] = create_socket_pair();
    if (client == SRT_INVALID_SOCK || server == SRT_INVALID_SOCK) {
        GTEST_SKIP() << "Failed to create socket pair";
    }

    bool test_completed = false;
    bool timeout_occurred = false;
    std::exception_ptr test_exception;

    asio::co_spawn(
        reactor_->get_io_context(),
        [&]() -> asio::awaitable<void> {
            try {
                // Wait for readable with 100ms timeout - should timeout since no data is sent
                auto start = std::chrono::steady_clock::now();
                co_await reactor_->async_wait_readable(server, 100ms);
                auto elapsed = std::chrono::steady_clock::now() - start;
                
                // Should not reach here - should have thrown
                ADD_FAILURE() << "Expected timeout exception";
            } catch (const asio::system_error& e) {
                // Check it's a timeout error
                if (e.code() == std::make_error_code(std::errc::timed_out)) {
                    timeout_occurred = true;
                    test_completed = true;
                } else {
                    test_exception = std::current_exception();
                }
            } catch (...) {
                test_exception = std::current_exception();
            }
        },
        asio::detached
    );

    // Wait for test to complete
    auto start = std::chrono::steady_clock::now();
    while (!test_completed && 
           std::chrono::steady_clock::now() - start < 2s) {
        std::this_thread::sleep_for(10ms);
    }

    if (test_exception) {
        std::rethrow_exception(test_exception);
    }

    EXPECT_TRUE(test_completed) << "Test did not complete in time";
    EXPECT_TRUE(timeout_occurred) << "Expected timeout error";
}

// Test 10: Timeout on readable - should succeed before timeout
TEST_F(SrtReactorTest, ReadableBeforeTimeout) {
    auto [client, server] = create_socket_pair();
    if (client == SRT_INVALID_SOCK || server == SRT_INVALID_SOCK) {
        GTEST_SKIP() << "Failed to create socket pair";
    }

    bool test_completed = false;
    int result_value = 0;
    std::exception_ptr test_exception;
    const char* msg = "test data";

    // Send data first
    asio::co_spawn(
        reactor_->get_io_context(),
        [&]() -> asio::awaitable<void> {
            try {
                co_await reactor_->async_wait_writable(client);
                srt_send(client, msg, strlen(msg));
            } catch (...) {
                test_exception = std::current_exception();
            }
        },
        asio::detached
    );

    // Small delay to ensure data is sent
    std::this_thread::sleep_for(50ms);

    // Now wait with timeout - should complete before timeout
    asio::co_spawn(
        reactor_->get_io_context(),
        [&]() -> asio::awaitable<void> {
            try {
                // Wait for readable with 1000ms timeout - should complete quickly
                auto start = std::chrono::steady_clock::now();
                result_value = co_await reactor_->async_wait_readable(server, 1000ms);
                auto elapsed = std::chrono::steady_clock::now() - start;
                
                // Should not timeout (result >= 0)
                EXPECT_GE(result_value, 0) << "Should not have timed out";
                
                // Should complete much faster than timeout
                auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
                EXPECT_LT(elapsed_ms, 500) << "Took too long, expected quick completion";
                
                test_completed = true;
            } catch (...) {
                test_exception = std::current_exception();
            }
        },
        asio::detached
    );

    // Wait for test to complete
    auto start = std::chrono::steady_clock::now();
    while (!test_completed && 
           std::chrono::steady_clock::now() - start < 3s) {
        std::this_thread::sleep_for(10ms);
    }

    if (test_exception) {
        std::rethrow_exception(test_exception);
    }

    EXPECT_TRUE(test_completed) << "Test did not complete in time";
}

// Test 11: Timeout on writable - should succeed immediately
TEST_F(SrtReactorTest, WritableWithTimeout) {
    auto [client, server] = create_socket_pair();
    if (client == SRT_INVALID_SOCK || server == SRT_INVALID_SOCK) {
        GTEST_SKIP() << "Failed to create socket pair";
    }

    bool test_completed = false;
    int result_value = 0;
    std::exception_ptr test_exception;

    asio::co_spawn(
        reactor_->get_io_context(),
        [&]() -> asio::awaitable<void> {
            try {
                // Socket should be writable immediately
                auto start = std::chrono::steady_clock::now();
                result_value = co_await reactor_->async_wait_writable(client, 1000ms);
                auto elapsed = std::chrono::steady_clock::now() - start;
                
                // Should not timeout (result >= 0)
                EXPECT_GE(result_value, 0) << "Should not have timed out";
                
                // Should complete very quickly
                auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
                EXPECT_LT(elapsed_ms, 100) << "Writable should be immediate";
                
                test_completed = true;
            } catch (const asio::system_error& e) {
                // Writable timeout would be unexpected
                if (e.code() == std::make_error_code(std::errc::timed_out)) {
                    ADD_FAILURE() << "Unexpected timeout on writable socket";
                }
                test_exception = std::current_exception();
            } catch (...) {
                test_exception = std::current_exception();
            }
        },
        asio::detached
    );

    // Wait for test to complete
    auto start = std::chrono::steady_clock::now();
    while (!test_completed && 
           std::chrono::steady_clock::now() - start < 2s) {
        std::this_thread::sleep_for(10ms);
    }

    if (test_exception) {
        std::rethrow_exception(test_exception);
    }

    EXPECT_TRUE(test_completed) << "Test did not complete in time";
}

// Test 12: 错误时通知所有等待者
TEST_F(SrtReactorTest, ErrorNotifiesAllWaiters) {
    auto [client, server] = create_socket_pair();
    if (client == SRT_INVALID_SOCK || server == SRT_INVALID_SOCK) {
        GTEST_SKIP() << "Failed to create socket pair";
    }

    std::atomic<int> read_notified{0};
    std::atomic<int> write_notified{0};
    std::exception_ptr test_exception;

    // 启动读等待者
    asio::co_spawn(
        reactor_->get_io_context(),
        [&]() -> asio::awaitable<void> {
            try {
                co_await reactor_->async_wait_readable(server);
                read_notified++;
            } catch (const asio::system_error& e) {
                // 预期收到错误
                read_notified++;
            } catch (...) {
                test_exception = std::current_exception();
            }
        },
        asio::detached
    );

    // 启动写等待者（可能立即完成或收到错误）
    asio::co_spawn(
        reactor_->get_io_context(),
        [&]() -> asio::awaitable<void> {
            try {
                co_await reactor_->async_wait_writable(server);
                write_notified++;
            } catch (const asio::system_error& e) {
                write_notified++;
            } catch (...) {
                test_exception = std::current_exception();
            }
        },
        asio::detached
    );

    // 给等待者一些时间注册
    std::this_thread::sleep_for(100ms);

    // 关闭 client 端，可能触发 server 端的错误
    srt_close(client);

    // 等待处理完成
    auto start = std::chrono::steady_clock::now();
    while ((read_notified == 0 || write_notified == 0) && 
           std::chrono::steady_clock::now() - start < 2s) {
        std::this_thread::sleep_for(20ms);
    }

    if (test_exception) {
        std::rethrow_exception(test_exception);
    }

    // 至少读等待者应该收到通知
    EXPECT_GT(read_notified, 0) << "Read waiter should be notified";
    
    // 清理
    if (server != SRT_INVALID_SOCK) {
        srt_close(server);
    }
}

// Test 13: 连接断开时的错误检测
TEST_F(SrtReactorTest, DetectConnectionLost) {
    auto [client, server] = create_socket_pair();
    if (client == SRT_INVALID_SOCK || server == SRT_INVALID_SOCK) {
        GTEST_SKIP() << "Failed to create socket pair";
    }

    bool event_received = false;
    bool is_error = false;
    std::exception_ptr test_exception;

    // 等待可读
    asio::co_spawn(
        reactor_->get_io_context(),
        [&]() -> asio::awaitable<void> {
            try {
                co_await reactor_->async_wait_readable(server);
                event_received = true;
                
                // 如果到这里（没有异常），尝试读取
                char buffer[100];
                int n = srt_recv(server, buffer, sizeof(buffer));
                if (n <= 0) {
                    // 读取失败，说明有错误
                    is_error = true;
                }
            } catch (const asio::system_error& e) {
                // 收到错误异常
                event_received = true;
                is_error = true;
            } catch (...) {
                test_exception = std::current_exception();
            }
        },
        asio::detached
    );

    // 给等待者时间注册
    std::this_thread::sleep_for(100ms);

    // 强制关闭连接
    srt_close(client);

    // 等待事件被处理
    auto start = std::chrono::steady_clock::now();
    while (!event_received && 
           std::chrono::steady_clock::now() - start < 2s) {
        std::this_thread::sleep_for(20ms);
    }

    if (test_exception) {
        std::rethrow_exception(test_exception);
    }

    // 应该收到某种通知（可能是错误，也可能是可读后发现错误）
    EXPECT_TRUE(event_received) << "Should receive some notification";

    // 清理
    if (server != SRT_INVALID_SOCK) {
        srt_close(server);
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

