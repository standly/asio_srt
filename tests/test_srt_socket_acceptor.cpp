// test_srt_socket_acceptor.cpp - 测试SrtSocket和SrtAcceptor的功能
#include <gtest/gtest.h>
#include "asrt/srt_reactor.hpp"
#include "asrt/srt_socket.hpp"
#include "asrt/srt_acceptor.hpp"
#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <thread>
#include <chrono>
#include <future>

using namespace asrt;
using namespace std::chrono_literals;

class SrtSocketAcceptorTest : public ::testing::Test {
protected:
    void SetUp() override {
        reactor_ = &SrtReactor::get_instance();
    }
    
    void TearDown() override {
        // 清理所有创建的socket
    }
    
    SrtReactor* reactor_;
};

// Test 1: 基本的连接和接受测试
TEST_F(SrtSocketAcceptorTest, BasicConnectAccept) {
    bool test_passed = false;
    std::exception_ptr test_exception;
    
    asio::co_spawn(
        reactor_->get_io_context(),
        [&]() -> asio::awaitable<void> {
            try {
                // 创建acceptor并绑定
                SrtAcceptor acceptor;
                acceptor.bind("127.0.0.1", 0);  // 使用随机端口
                
                // 获取实际绑定的端口
                auto local_addr = acceptor.get_local_address();
                uint16_t port = local_addr.second;
                
                // 创建客户端socket
                SrtSocket client;
                
                // 启动异步接受
                auto accept_future = asio::co_spawn(
                    reactor_->get_io_context(),
                    [&]() -> asio::awaitable<SrtSocket> {
                        co_return co_await acceptor.async_accept();
                    },
                    asio::use_future
                );
                
                // 等待一下确保acceptor准备好
                co_await asio::steady_timer(reactor_->get_io_context(), 50ms).async_wait();
                
                // 客户端连接
                co_await client.async_connect("127.0.0.1", port);
                
                // 等待接受完成
                SrtSocket server = co_await accept_future;
                
                // 验证连接成功
                EXPECT_TRUE(client.is_connected());
                EXPECT_TRUE(server.is_connected());
                
                test_passed = true;
            } catch (...) {
                test_exception = std::current_exception();
            }
        },
        asio::detached
    );
    
    // 等待测试完成
    auto start = std::chrono::steady_clock::now();
    while (!test_passed && !test_exception &&
           std::chrono::steady_clock::now() - start < 5s) {
        std::this_thread::sleep_for(10ms);
    }
    
    if (test_exception) {
        std::rethrow_exception(test_exception);
    }
    
    EXPECT_TRUE(test_passed) << "测试应该成功完成";
}

// Test 2: 测试listener callback
TEST_F(SrtSocketAcceptorTest, ListenerCallback) {
    bool callback_called = false;
    bool test_passed = false;
    std::string received_streamid;
    std::exception_ptr test_exception;
    
    asio::co_spawn(
        reactor_->get_io_context(),
        [&]() -> asio::awaitable<void> {
            try {
                SrtAcceptor acceptor;
                
                // 设置listener callback
                acceptor.set_listener_callback(
                    [&](SrtSocket& socket, int hsversion, const std::string& streamid) {
                        callback_called = true;
                        received_streamid = streamid;
                        
                        // 可以在这里设置socket选项
                        socket.set_option("SRTO_RCVBUF", "65536");
                        
                        // 返回0接受连接，-1拒绝
                        return 0;
                    }
                );
                
                acceptor.bind("127.0.0.1", 0);
                auto port = acceptor.get_local_address().second;
                
                // 客户端设置stream ID
                SrtSocket client;
                client.set_option("SRTO_STREAMID", "test-stream-123");
                
                // 启动异步接受
                auto accept_future = asio::co_spawn(
                    reactor_->get_io_context(),
                    acceptor.async_accept(),
                    asio::use_future
                );
                
                co_await asio::steady_timer(reactor_->get_io_context(), 50ms).async_wait();
                
                // 连接
                co_await client.async_connect("127.0.0.1", port);
                
                // 等待接受
                SrtSocket server = co_await accept_future;
                
                // 验证callback被调用
                EXPECT_TRUE(callback_called);
                EXPECT_EQ(received_streamid, "test-stream-123");
                
                test_passed = true;
            } catch (...) {
                test_exception = std::current_exception();
            }
        },
        asio::detached
    );
    
    auto start = std::chrono::steady_clock::now();
    while (!test_passed && !test_exception &&
           std::chrono::steady_clock::now() - start < 5s) {
        std::this_thread::sleep_for(10ms);
    }
    
    if (test_exception) {
        std::rethrow_exception(test_exception);
    }
    
    EXPECT_TRUE(test_passed);
}

// Test 3: 测试连接拒绝
TEST_F(SrtSocketAcceptorTest, ConnectionRejection) {
    bool test_passed = false;
    std::exception_ptr test_exception;
    
    asio::co_spawn(
        reactor_->get_io_context(),
        [&]() -> asio::awaitable<void> {
            try {
                SrtAcceptor acceptor;
                
                // 设置拒绝所有连接的callback
                acceptor.set_listener_callback(
                    [](SrtSocket&, int, const std::string&) {
                        return -1;  // 拒绝连接
                    }
                );
                
                acceptor.bind("127.0.0.1", 0);
                auto port = acceptor.get_local_address().second;
                
                SrtSocket client;
                
                // 尝试连接（应该失败）
                bool connect_failed = false;
                try {
                    co_await client.async_connect("127.0.0.1", port);
                } catch (const std::system_error& e) {
                    connect_failed = true;
                }
                
                EXPECT_TRUE(connect_failed) << "连接应该被拒绝";
                EXPECT_FALSE(client.is_connected());
                
                test_passed = true;
            } catch (...) {
                test_exception = std::current_exception();
            }
        },
        asio::detached
    );
    
    auto start = std::chrono::steady_clock::now();
    while (!test_passed && !test_exception &&
           std::chrono::steady_clock::now() - start < 5s) {
        std::this_thread::sleep_for(10ms);
    }
    
    if (test_exception) {
        std::rethrow_exception(test_exception);
    }
    
    EXPECT_TRUE(test_passed);
}

// Test 4: 测试socket选项设置和获取
TEST_F(SrtSocketAcceptorTest, SocketOptions) {
    bool test_passed = false;
    std::exception_ptr test_exception;
    
    asio::co_spawn(
        reactor_->get_io_context(),
        [&]() -> asio::awaitable<void> {
            try {
                SrtSocket socket;
                
                // 设置各种选项
                socket.set_option("SRTO_SNDBUF", "1048576");  // 1MB
                socket.set_option("SRTO_RCVBUF", "2097152");  // 2MB
                socket.set_option("SRTO_FC", "256");
                socket.set_option("SRTO_MSS", "1000");
                
                // 获取并验证选项
                auto sndbuf = socket.get_option("SRTO_SNDBUF");
                auto rcvbuf = socket.get_option("SRTO_RCVBUF");
                auto fc = socket.get_option("SRTO_FC");
                auto mss = socket.get_option("SRTO_MSS");
                
                EXPECT_EQ(sndbuf, "1048576");
                EXPECT_EQ(rcvbuf, "2097152");
                EXPECT_EQ(fc, "256");
                EXPECT_EQ(mss, "1000");
                
                test_passed = true;
            } catch (...) {
                test_exception = std::current_exception();
            }
        },
        asio::detached
    );
    
    auto start = std::chrono::steady_clock::now();
    while (!test_passed && !test_exception &&
           std::chrono::steady_clock::now() - start < 5s) {
        std::this_thread::sleep_for(10ms);
    }
    
    if (test_exception) {
        std::rethrow_exception(test_exception);
    }
    
    EXPECT_TRUE(test_passed);
}

// Test 5: 测试数据发送和接收
TEST_F(SrtSocketAcceptorTest, DataTransfer) {
    bool test_passed = false;
    std::exception_ptr test_exception;
    
    asio::co_spawn(
        reactor_->get_io_context(),
        [&]() -> asio::awaitable<void> {
            try {
                // 设置连接
                SrtAcceptor acceptor;
                acceptor.bind("127.0.0.1", 0);
                auto port = acceptor.get_local_address().second;
                
                SrtSocket client;
                
                auto accept_future = asio::co_spawn(
                    reactor_->get_io_context(),
                    acceptor.async_accept(),
                    asio::use_future
                );
                
                co_await asio::steady_timer(reactor_->get_io_context(), 50ms).async_wait();
                co_await client.async_connect("127.0.0.1", port);
                SrtSocket server = co_await accept_future;
                
                // 测试数据传输
                const std::string test_data = "Hello, SRT! This is a test message.";
                
                // 客户端发送
                auto sent = co_await client.async_send(
                    asio::buffer(test_data.data(), test_data.size())
                );
                EXPECT_EQ(sent, test_data.size());
                
                // 服务器接收
                char recv_buffer[1024];
                auto received = co_await server.async_receive(
                    asio::buffer(recv_buffer, sizeof(recv_buffer))
                );
                
                EXPECT_EQ(received, test_data.size());
                EXPECT_EQ(std::string(recv_buffer, received), test_data);
                
                // 反向测试：服务器发送，客户端接收
                const std::string reply = "Reply from server";
                sent = co_await server.async_send(
                    asio::buffer(reply.data(), reply.size())
                );
                EXPECT_EQ(sent, reply.size());
                
                received = co_await client.async_receive(
                    asio::buffer(recv_buffer, sizeof(recv_buffer))
                );
                EXPECT_EQ(received, reply.size());
                EXPECT_EQ(std::string(recv_buffer, received), reply);
                
                test_passed = true;
            } catch (...) {
                test_exception = std::current_exception();
            }
        },
        asio::detached
    );
    
    auto start = std::chrono::steady_clock::now();
    while (!test_passed && !test_exception &&
           std::chrono::steady_clock::now() - start < 5s) {
        std::this_thread::sleep_for(10ms);
    }
    
    if (test_exception) {
        std::rethrow_exception(test_exception);
    }
    
    EXPECT_TRUE(test_passed);
}

// Test 6: 测试超时连接
TEST_F(SrtSocketAcceptorTest, ConnectTimeout) {
    bool test_passed = false;
    std::exception_ptr test_exception;
    
    asio::co_spawn(
        reactor_->get_io_context(),
        [&]() -> asio::awaitable<void> {
            try {
                SrtSocket client;
                
                // 尝试连接到一个不存在的地址
                bool timeout_occurred = false;
                auto start_time = std::chrono::steady_clock::now();
                
                try {
                    // 使用较短的超时时间
                    co_await client.async_connect("192.168.255.255", 12345, 1s);
                } catch (const std::system_error& e) {
                    timeout_occurred = true;
                    auto duration = std::chrono::steady_clock::now() - start_time;
                    
                    // 验证确实是超时
                    EXPECT_LT(duration, 2s) << "应该在2秒内超时";
                    EXPECT_GT(duration, 500ms) << "超时时间应该至少500ms";
                }
                
                EXPECT_TRUE(timeout_occurred) << "应该发生超时";
                EXPECT_FALSE(client.is_connected());
                
                test_passed = true;
            } catch (...) {
                test_exception = std::current_exception();
            }
        },
        asio::detached
    );
    
    auto start = std::chrono::steady_clock::now();
    while (!test_passed && !test_exception &&
           std::chrono::steady_clock::now() - start < 10s) {
        std::this_thread::sleep_for(10ms);
    }
    
    if (test_exception) {
        std::rethrow_exception(test_exception);
    }
    
    EXPECT_TRUE(test_passed);
}

// Test 7: 测试多个并发连接
TEST_F(SrtSocketAcceptorTest, MultipleConcurrentConnections) {
    bool test_passed = false;
    std::exception_ptr test_exception;
    const int num_clients = 5;
    
    asio::co_spawn(
        reactor_->get_io_context(),
        [&]() -> asio::awaitable<void> {
            try {
                SrtAcceptor acceptor;
                acceptor.bind("127.0.0.1", 0);
                auto port = acceptor.get_local_address().second;
                
                std::vector<SrtSocket> servers;
                std::vector<std::future<SrtSocket>> accept_futures;
                
                // 启动多个accept操作
                for (int i = 0; i < num_clients; ++i) {
                    accept_futures.push_back(
                        asio::co_spawn(
                            reactor_->get_io_context(),
                            acceptor.async_accept(),
                            asio::use_future
                        )
                    );
                }
                
                // 等待acceptor准备好
                co_await asio::steady_timer(reactor_->get_io_context(), 100ms).async_wait();
                
                // 创建多个客户端并连接
                std::vector<SrtSocket> clients(num_clients);
                std::vector<std::future<void>> connect_futures;
                
                for (int i = 0; i < num_clients; ++i) {
                    connect_futures.push_back(
                        asio::co_spawn(
                            reactor_->get_io_context(),
                            [&, i]() -> asio::awaitable<void> {
                                co_await clients[i].async_connect("127.0.0.1", port);
                            },
                            asio::use_future
                        )
                    );
                }
                
                // 等待所有连接完成
                for (auto& future : connect_futures) {
                    co_await future;
                }
                
                // 收集所有接受的连接
                for (auto& future : accept_futures) {
                    servers.push_back(co_await future);
                }
                
                // 验证所有连接
                EXPECT_EQ(servers.size(), num_clients);
                for (int i = 0; i < num_clients; ++i) {
                    EXPECT_TRUE(clients[i].is_connected());
                    EXPECT_TRUE(servers[i].is_connected());
                }
                
                test_passed = true;
            } catch (...) {
                test_exception = std::current_exception();
            }
        },
        asio::detached
    );
    
    auto start = std::chrono::steady_clock::now();
    while (!test_passed && !test_exception &&
           std::chrono::steady_clock::now() - start < 10s) {
        std::this_thread::sleep_for(10ms);
    }
    
    if (test_exception) {
        std::rethrow_exception(test_exception);
    }
    
    EXPECT_TRUE(test_passed);
}

// Test 8: 测试connect callback
TEST_F(SrtSocketAcceptorTest, ConnectCallback) {
    bool callback_called = false;
    bool test_passed = false;
    std::exception_ptr test_exception;
    std::error_code callback_error;
    
    asio::co_spawn(
        reactor_->get_io_context(),
        [&]() -> asio::awaitable<void> {
            try {
                // 设置服务器
                SrtAcceptor acceptor;
                acceptor.bind("127.0.0.1", 0);
                auto port = acceptor.get_local_address().second;
                
                SrtSocket client;
                
                // 设置connect callback
                client.set_connect_callback(
                    [&](const std::error_code& ec, SrtSocket& socket) {
                        callback_called = true;
                        callback_error = ec;
                        
                        if (!ec) {
                            // 连接成功，可以立即发送数据
                            const char* msg = "Hello from callback";
                            srt_send(socket.native_handle(), msg, strlen(msg));
                        }
                    }
                );
                
                // 启动accept
                auto accept_future = asio::co_spawn(
                    reactor_->get_io_context(),
                    acceptor.async_accept(),
                    asio::use_future
                );
                
                co_await asio::steady_timer(reactor_->get_io_context(), 50ms).async_wait();
                
                // 连接
                co_await client.async_connect("127.0.0.1", port);
                
                // 获取服务器端socket
                SrtSocket server = co_await accept_future;
                
                // 验证callback被调用
                EXPECT_TRUE(callback_called);
                EXPECT_FALSE(callback_error) << "Callback error: " << callback_error.message();
                
                // 验证从callback发送的数据
                char buffer[256];
                auto received = co_await server.async_receive(asio::buffer(buffer));
                EXPECT_GT(received, 0);
                EXPECT_EQ(std::string(buffer, received), "Hello from callback");
                
                test_passed = true;
            } catch (...) {
                test_exception = std::current_exception();
            }
        },
        asio::detached
    );
    
    auto start = std::chrono::steady_clock::now();
    while (!test_passed && !test_exception &&
           std::chrono::steady_clock::now() - start < 5s) {
        std::this_thread::sleep_for(10ms);
    }
    
    if (test_exception) {
        std::rethrow_exception(test_exception);
    }
    
    EXPECT_TRUE(test_passed);
}

// Test 9: 测试地址获取功能
TEST_F(SrtSocketAcceptorTest, AddressOperations) {
    bool test_passed = false;
    std::exception_ptr test_exception;
    
    asio::co_spawn(
        reactor_->get_io_context(),
        [&]() -> asio::awaitable<void> {
            try {
                // Acceptor地址测试
                SrtAcceptor acceptor;
                acceptor.bind("127.0.0.1", 0);
                
                auto local_addr = acceptor.get_local_address();
                EXPECT_EQ(local_addr.first, "127.0.0.1");
                EXPECT_GT(local_addr.second, 0);
                
                // Socket地址测试
                SrtSocket client;
                auto port = local_addr.second;
                
                auto accept_future = asio::co_spawn(
                    reactor_->get_io_context(),
                    acceptor.async_accept(),
                    asio::use_future
                );
                
                co_await asio::steady_timer(reactor_->get_io_context(), 50ms).async_wait();
                co_await client.async_connect("127.0.0.1", port);
                SrtSocket server = co_await accept_future;
                
                // 获取客户端地址
                auto client_local = client.get_local_address();
                auto client_peer = client.get_peer_address();
                
                EXPECT_EQ(client_local.first, "127.0.0.1");
                EXPECT_GT(client_local.second, 0);
                EXPECT_EQ(client_peer.first, "127.0.0.1");
                EXPECT_EQ(client_peer.second, port);
                
                // 获取服务器端地址
                auto server_local = server.get_local_address();
                auto server_peer = server.get_peer_address();
                
                EXPECT_EQ(server_local.first, "127.0.0.1");
                EXPECT_EQ(server_local.second, port);
                EXPECT_EQ(server_peer.first, "127.0.0.1");
                EXPECT_EQ(server_peer.second, client_local.second);
                
                test_passed = true;
            } catch (...) {
                test_exception = std::current_exception();
            }
        },
        asio::detached
    );
    
    auto start = std::chrono::steady_clock::now();
    while (!test_passed && !test_exception &&
           std::chrono::steady_clock::now() - start < 5s) {
        std::this_thread::sleep_for(10ms);
    }
    
    if (test_exception) {
        std::rethrow_exception(test_exception);
    }
    
    EXPECT_TRUE(test_passed);
}

// Test 10: 测试错误处理
TEST_F(SrtSocketAcceptorTest, ErrorHandling) {
    // 测试无效操作
    SrtSocket socket;
    
    // 未连接时发送应该失败
    bool send_failed = false;
    asio::co_spawn(
        reactor_->get_io_context(),
        [&]() -> asio::awaitable<void> {
            try {
                const char* data = "test";
                co_await socket.async_send(asio::buffer(data, 4));
            } catch (const std::system_error&) {
                send_failed = true;
            }
        },
        asio::detached
    );
    
    std::this_thread::sleep_for(100ms);
    EXPECT_TRUE(send_failed) << "未连接的socket发送应该失败";
    
    // 测试重复绑定
    SrtAcceptor acceptor1, acceptor2;
    acceptor1.bind("127.0.0.1", 0);
    auto port = acceptor1.get_local_address().second;
    
    bool bind_failed = false;
    try {
        acceptor2.bind("127.0.0.1", port);
    } catch (const std::system_error&) {
        bind_failed = true;
    }
    
    EXPECT_TRUE(bind_failed) << "重复绑定相同端口应该失败";
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
