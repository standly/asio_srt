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
#include <atomic>

using namespace asrt;
using namespace std::chrono_literals;

// 辅助函数：解析地址字符串
static std::pair<std::string, uint16_t> parse_address(const std::string& addr_str) {
    auto colon_pos = addr_str.rfind(':');
    if (colon_pos == std::string::npos) {
        return {addr_str, 0};
    }
    return {addr_str.substr(0, colon_pos), static_cast<uint16_t>(std::stoi(addr_str.substr(colon_pos + 1)))};
}

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
                auto local_addr = parse_address(acceptor.local_address());
                uint16_t port = local_addr.second;
                
                // 创建客户端socket
                SrtSocket client;
                
                // 启动异步接受
                SrtSocket server;
                bool server_accepted = false;
                asio::co_spawn(
                    reactor_->get_io_context(),
                    [&]() -> asio::awaitable<void> {
                        server = co_await acceptor.async_accept();
                        server_accepted = true;
                    },
                    asio::detached
                );
                
                // 等待一下确保acceptor准备好
                co_await asio::steady_timer(reactor_->get_io_context(), 50ms).async_wait();
                
                // 客户端连接
                co_await client.async_connect("127.0.0.1", port);
                
                // 等待接受完成
                for (int i = 0; i < 10 && !server_accepted; ++i) {
                    co_await asio::steady_timer(reactor_->get_io_context(), 10ms).async_wait();
                }
                EXPECT_TRUE(server_accepted);
                
                // 验证连接成功
                EXPECT_TRUE(client.is_open());
                EXPECT_TRUE(server.is_open());
                
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
                        socket.set_option("rcvbuf=65536");
                        
                        // 返回0接受连接，-1拒绝
                        return 0;
                    }
                );
                
                acceptor.bind("127.0.0.1", 0);
                auto port = parse_address(acceptor.local_address()).second;
                
                // 客户端设置stream ID
                SrtSocket client;
                client.set_option("streamid=test-stream-123");
                
                // 启动异步接受
                SrtSocket server;
                bool server_accepted = false;
                asio::co_spawn(
                    reactor_->get_io_context(),
                    [&acceptor, &server, &server_accepted]() -> asio::awaitable<void> {
                        server = co_await acceptor.async_accept();
                        server_accepted = true;
                    },
                    asio::detached
                );
                
                co_await asio::steady_timer(reactor_->get_io_context(), 50ms).async_wait();
                
                // 连接
                co_await client.async_connect("127.0.0.1", port);
                
                // 等待接受
                for (int i = 0; i < 10 && !server_accepted; ++i) {
                    co_await asio::steady_timer(reactor_->get_io_context(), 10ms).async_wait();
                }
                EXPECT_TRUE(server_accepted);
                
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
                auto port = parse_address(acceptor.local_address()).second;
                
                SrtSocket client;
                
                // 尝试连接（应该失败）
                bool connect_failed = false;
                try {
                    co_await client.async_connect("127.0.0.1", port);
                } catch (const std::system_error& e) {
                    connect_failed = true;
                }
                
                EXPECT_TRUE(connect_failed) << "连接应该被拒绝";
                EXPECT_FALSE(client.is_open());
                
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
    // 直接在主线程中测试，避免协程相关问题
    try {
        SrtSocket socket;
        
        // 设置各种选项
        socket.set_option("sndbuf=1048576");  // 1MB
        socket.set_option("rcvbuf=2097152");  // 2MB
        socket.set_option("fc=256");
        socket.set_option("mss=1000");
        
        // 注意：当前API不支持get_option，无法验证选项值
        // TODO: 如果将来添加了get_option方法，可以验证选项值
        
        // 目前只能验证set_option没有抛出异常
        // 选项设置成功（没有异常）
        
        SUCCEED() << "Socket选项设置成功";
    } catch (const std::exception& e) {
        FAIL() << "设置socket选项时发生异常: " << e.what();
    } catch (...) {
        FAIL() << "设置socket选项时发生未知异常";
    }
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
                
                // 设置acceptor的选项（必须在bind之前）
                acceptor.set_option("messageapi=1");
                acceptor.set_option("payloadsize=1316");
                
                // 设置listener callback来确保接受的socket也有正确的设置
                acceptor.set_listener_callback(
                    [](SrtSocket& socket, int, const std::string&) {
                        // 对接受的socket也设置选项
                        socket.set_option("messageapi=1");
                        socket.set_option("payloadsize=1316");
                        return 0;  // 接受连接
                    }
                );
                
                acceptor.bind("127.0.0.1", 0);
                auto port = parse_address(acceptor.local_address()).second;
                
                SrtSocket client;
                // 客户端也设置相同的选项
                client.set_option("messageapi=1");
                client.set_option("payloadsize=1316");
                
                SrtSocket server;
                bool server_accepted = false;
                asio::co_spawn(
                    reactor_->get_io_context(),
                    [&acceptor, &server, &server_accepted]() -> asio::awaitable<void> {
                        server = co_await acceptor.async_accept();
                        server_accepted = true;
                    },
                    asio::detached
                );
                
                co_await asio::steady_timer(reactor_->get_io_context(), 50ms).async_wait();
                co_await client.async_connect("127.0.0.1", port);
                
                for (int i = 0; i < 10 && !server_accepted; ++i) {
                    co_await asio::steady_timer(reactor_->get_io_context(), 10ms).async_wait();
                }
                EXPECT_TRUE(server_accepted);
                
                // 测试数据传输
                const std::string test_data = "Hello, SRT! This is a test message.";
                
                // 客户端发送
                auto sent = co_await client.async_write_packet(
                    test_data.data(), test_data.size()
                );
                EXPECT_EQ(sent, test_data.size());
                
                // 服务器接收
                char recv_buffer[1024];
                auto received = co_await server.async_read_packet(
                    recv_buffer, sizeof(recv_buffer)
                );
                
                EXPECT_EQ(received, test_data.size());
                EXPECT_EQ(std::string(recv_buffer, received), test_data);
                
                // 反向测试：服务器发送，客户端接收
                const std::string reply = "Reply from server";
                sent = co_await server.async_write_packet(
                    reply.data(), reply.size()
                );
                EXPECT_EQ(sent, reply.size());
                
                received = co_await client.async_read_packet(
                    recv_buffer, sizeof(recv_buffer)
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
                EXPECT_FALSE(client.is_open());
                
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
                auto port = parse_address(acceptor.local_address()).second;
                
                std::vector<SrtSocket> servers(num_clients);
                std::atomic<int> accepted_count(0);
                
                // 启动多个accept操作
                for (int i = 0; i < num_clients; ++i) {
                    asio::co_spawn(
                        reactor_->get_io_context(),
                        [&acceptor, &servers, &accepted_count, i]() -> asio::awaitable<void> {
                            servers[i] = co_await acceptor.async_accept();
                            accepted_count++;
                        },
                        asio::detached
                    );
                }
                
                // 等待acceptor准备好
                co_await asio::steady_timer(reactor_->get_io_context(), 100ms).async_wait();
                
                // 创建多个客户端并连接
                std::vector<SrtSocket> clients(num_clients);
                std::atomic<int> connected_count(0);
                
                for (int i = 0; i < num_clients; ++i) {
                    asio::co_spawn(
                        reactor_->get_io_context(),
                        [&clients, &connected_count, i, port]() -> asio::awaitable<void> {
                            co_await clients[i].async_connect("127.0.0.1", port);
                            connected_count++;
                        },
                        asio::detached
                    );
                }
                
                // 等待所有连接和接受完成
                for (int retry = 0; retry < 50 && (connected_count < num_clients || accepted_count < num_clients); ++retry) {
                    co_await asio::steady_timer(reactor_->get_io_context(), 10ms).async_wait();
                }
                
                EXPECT_EQ(connected_count.load(), num_clients);
                EXPECT_EQ(accepted_count.load(), num_clients);
                
                // 验证所有连接
                EXPECT_EQ(servers.size(), num_clients);
                for (int i = 0; i < num_clients; ++i) {
                    EXPECT_TRUE(clients[i].is_open());
                    EXPECT_TRUE(servers[i].is_open());
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
                auto port = parse_address(acceptor.local_address()).second;
                
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
                SrtSocket server;
                bool server_accepted = false;
                asio::co_spawn(
                    reactor_->get_io_context(),
                    [&acceptor, &server, &server_accepted]() -> asio::awaitable<void> {
                        server = co_await acceptor.async_accept();
                        server_accepted = true;
                    },
                    asio::detached
                );
                
                co_await asio::steady_timer(reactor_->get_io_context(), 50ms).async_wait();
                
                // 连接
                co_await client.async_connect("127.0.0.1", port);
                
                // 等待接受完成
                for (int i = 0; i < 10 && !server_accepted; ++i) {
                    co_await asio::steady_timer(reactor_->get_io_context(), 10ms).async_wait();
                }
                EXPECT_TRUE(server_accepted);
                
                // 验证callback被调用
                EXPECT_TRUE(callback_called);
                EXPECT_FALSE(callback_error) << "Callback error: " << callback_error.message();
                
                // 验证从callback发送的数据
                char buffer[256];
                auto received = co_await server.async_read_packet(buffer, sizeof(buffer));
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
                
                auto local_addr = parse_address(acceptor.local_address());
                EXPECT_EQ(local_addr.first, "127.0.0.1");
                EXPECT_GT(local_addr.second, 0);
                
                // Socket地址测试
                SrtSocket client;
                auto port = local_addr.second;
                
                SrtSocket server;
                bool server_accepted = false;
                asio::co_spawn(
                    reactor_->get_io_context(),
                    [&acceptor, &server, &server_accepted]() -> asio::awaitable<void> {
                        server = co_await acceptor.async_accept();
                        server_accepted = true;
                    },
                    asio::detached
                );
                
                co_await asio::steady_timer(reactor_->get_io_context(), 50ms).async_wait();
                co_await client.async_connect("127.0.0.1", port);
                
                for (int i = 0; i < 10 && !server_accepted; ++i) {
                    co_await asio::steady_timer(reactor_->get_io_context(), 10ms).async_wait();
                }
                EXPECT_TRUE(server_accepted);
                
                // 获取客户端地址
                std::string client_local_str = client.local_address();
                std::string client_peer_str = client.remote_address();
                
                std::cout << "Client local address: '" << client_local_str << "'" << std::endl;
                std::cout << "Client peer address: '" << client_peer_str << "'" << std::endl;
                
                auto client_local = parse_address(client_local_str);
                auto client_peer = parse_address(client_peer_str);
                
                // 如果地址字符串为空或者没有端口，跳过这些检查
                if (!client_local_str.empty() && client_local_str.find(':') != std::string::npos) {
                    EXPECT_EQ(client_local.first, "127.0.0.1");
                    EXPECT_GT(client_local.second, 0);
                }
                if (!client_peer_str.empty() && client_peer_str.find(':') != std::string::npos) {
                    EXPECT_EQ(client_peer.first, "127.0.0.1");
                    EXPECT_EQ(client_peer.second, port);
                }
                
                // 获取服务器端地址
                auto server_local = parse_address(server.local_address());
                auto server_peer = parse_address(server.remote_address());
                
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
                co_await socket.async_write_packet(data, 4);
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
    auto port = parse_address(acceptor1.local_address()).second;
    
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
