// srt_streaming_example.cpp - SRT流媒体传输示例
// 展示如何使用asio_srt进行实时音视频流传输

#include "asrt/srt_reactor.hpp"
#include "asrt/srt_socket.hpp"
#include "asrt/srt_acceptor.hpp"
#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/steady_timer.hpp>
#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <vector>
#include <cstring>

using namespace asrt;
using namespace std::chrono_literals;

// 模拟的媒体数据包
struct MediaPacket {
    uint32_t timestamp;  // 时间戳
    uint16_t sequence;   // 序列号
    uint16_t size;       // 数据大小
    char data[1316];     // 数据 (SRT推荐的最大负载大小)
};

// ========================================
// 示例 1：实时视频流服务器
// ========================================

asio::awaitable<void> video_stream_server(uint16_t port) {
    std::cout << "\n=== 启动视频流服务器 ===" << std::endl;
    
    try {
        SrtAcceptor acceptor;
        
        // 设置服务器选项
        acceptor.set_option("SRTO_LATENCY", "120");      // 120ms延迟
        acceptor.set_option("SRTO_RCVBUF", "8192000");   // 8MB接收缓冲
        acceptor.set_option("SRTO_FC", "25600");         // 流控制窗口
        
        // 设置监听回调，可以根据stream ID路由不同的流
        acceptor.set_listener_callback(
            [](SrtSocket& socket, int hsversion, const std::string& streamid) {
                std::cout << "新连接请求 - Stream ID: " << streamid << std::endl;
                
                // 根据stream ID设置不同的参数
                if (streamid.find("hd") != std::string::npos) {
                    socket.set_option("SRTO_MAXBW", "20000000");  // HD: 20Mbps
                } else {
                    socket.set_option("SRTO_MAXBW", "5000000");   // SD: 5Mbps
                }
                
                return 0;  // 接受连接
            }
        );
        
        acceptor.bind("0.0.0.0", port);
        std::cout << "视频流服务器监听端口: " << port << std::endl;
        
        // 接受客户端连接
        while (true) {
            SrtSocket client = co_await acceptor.async_accept();
            
            auto [addr, port] = client.get_peer_address();
            std::cout << "客户端连接: " << addr << ":" << port << std::endl;
            
            // 为每个客户端启动独立的流任务
            asio::co_spawn(
                SrtReactor::get_instance().get_io_context(),
                [client = std::move(client)]() mutable -> asio::awaitable<void> {
                    try {
                        uint16_t sequence = 0;
                        asio::steady_timer timer(co_await asio::this_coro::executor);
                        
                        // 模拟 30fps 视频流
                        const auto frame_duration = 33ms;
                        
                        while (client.is_connected()) {
                            timer.expires_after(frame_duration);
                            co_await timer.async_wait();
                            
                            // 创建模拟视频帧
                            MediaPacket packet;
                            packet.timestamp = static_cast<uint32_t>(
                                std::chrono::steady_clock::now().time_since_epoch().count());
                            packet.sequence = sequence++;
                            packet.size = 1000;  // 模拟数据大小
                            
                            // 填充模拟数据
                            std::memset(packet.data, sequence % 256, packet.size);
                            
                            // 发送数据包
                            size_t sent = co_await client.async_send(
                                asio::buffer(&packet, sizeof(MediaPacket::timestamp) + 
                                           sizeof(MediaPacket::sequence) + 
                                           sizeof(MediaPacket::size) + packet.size)
                            );
                            
                            if (sequence % 30 == 0) {  // 每秒打印一次
                                std::cout << "已发送 " << sequence << " 帧" << std::endl;
                            }
                        }
                    } catch (const std::exception& e) {
                        std::cout << "流传输错误: " << e.what() << std::endl;
                    }
                    
                    std::cout << "客户端断开连接" << std::endl;
                },
                asio::detached
            );
        }
    } catch (const std::exception& e) {
        std::cout << "服务器错误: " << e.what() << std::endl;
    }
}

// ========================================
// 示例 2：视频流客户端
// ========================================

asio::awaitable<void> video_stream_client(const std::string& server_addr, 
                                         uint16_t server_port,
                                         const std::string& stream_id) {
    std::cout << "\n=== 启动视频流客户端 ===" << std::endl;
    
    try {
        SrtSocket socket;
        
        // 设置客户端选项
        socket.set_option("SRTO_LATENCY", "120");        // 120ms延迟
        socket.set_option("SRTO_SNDBUF", "8192000");     // 8MB发送缓冲
        socket.set_option("SRTO_STREAMID", stream_id);   // 设置流ID
        
        // 连接到服务器
        std::cout << "连接到 " << server_addr << ":" << server_port << std::endl;
        co_await socket.async_connect(server_addr, server_port);
        std::cout << "已连接到视频流服务器" << std::endl;
        
        // 接收统计
        uint64_t total_bytes = 0;
        uint32_t total_packets = 0;
        auto start_time = std::chrono::steady_clock::now();
        
        // 接收缓冲区
        std::vector<char> buffer(2048);
        
        while (socket.is_connected()) {
            // 接收数据包
            size_t received = co_await socket.async_receive(asio::buffer(buffer));
            
            if (received >= sizeof(MediaPacket) - sizeof(MediaPacket::data)) {
                MediaPacket* packet = reinterpret_cast<MediaPacket*>(buffer.data());
                
                total_bytes += received;
                total_packets++;
                
                // 每秒打印统计信息
                if (total_packets % 30 == 0) {
                    auto now = std::chrono::steady_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::seconds>(
                        now - start_time).count();
                    
                    if (duration > 0) {
                        double mbps = (total_bytes * 8.0 / 1000000.0) / duration;
                        std::cout << "接收统计 - 包数: " << total_packets 
                                 << ", 总字节: " << total_bytes
                                 << ", 平均速率: " << mbps << " Mbps" << std::endl;
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        std::cout << "客户端错误: " << e.what() << std::endl;
    }
}

// ========================================
// 示例 3：双向音视频通话
// ========================================

asio::awaitable<void> bidirectional_call_peer(uint16_t local_port, 
                                             const std::string& remote_addr,
                                             uint16_t remote_port,
                                             bool is_caller) {
    std::cout << "\n=== 启动双向通话" << (is_caller ? "（主叫）" : "（被叫）") 
              << " ===" << std::endl;
    
    try {
        // 创建本地监听
        SrtAcceptor acceptor;
        acceptor.set_option("SRTO_LATENCY", "60");  // 低延迟用于实时通话
        acceptor.bind("0.0.0.0", local_port);
        
        SrtSocket send_socket;
        SrtSocket recv_socket;
        
        if (is_caller) {
            // 主叫方：先连接对方，再等待对方连接
            send_socket.set_option("SRTO_LATENCY", "60");
            std::cout << "连接到对方..." << std::endl;
            co_await send_socket.async_connect(remote_addr, remote_port);
            std::cout << "等待对方连接..." << std::endl;
            recv_socket = co_await acceptor.async_accept();
        } else {
            // 被叫方：先等待对方连接，再连接对方
            std::cout << "等待对方连接..." << std::endl;
            recv_socket = co_await acceptor.async_accept();
            send_socket.set_option("SRTO_LATENCY", "60");
            std::cout << "连接到对方..." << std::endl;
            co_await send_socket.async_connect(remote_addr, remote_port);
        }
        
        std::cout << "双向连接建立成功！" << std::endl;
        
        // 启动发送任务（模拟音频）
        auto send_task = asio::co_spawn(
            SrtReactor::get_instance().get_io_context(),
            [send_socket = std::move(send_socket)]() mutable -> asio::awaitable<void> {
                try {
                    asio::steady_timer timer(co_await asio::this_coro::executor);
                    uint16_t seq = 0;
                    
                    // 模拟 20ms 音频帧
                    while (send_socket.is_connected()) {
                        timer.expires_after(20ms);
                        co_await timer.async_wait();
                        
                        // 模拟音频数据（160字节 = 20ms * 8kHz）
                        char audio_frame[160];
                        std::memset(audio_frame, seq % 256, sizeof(audio_frame));
                        
                        co_await send_socket.async_send(asio::buffer(audio_frame));
                        seq++;
                    }
                } catch (const std::exception& e) {
                    std::cout << "发送错误: " << e.what() << std::endl;
                }
            },
            asio::use_future
        );
        
        // 启动接收任务
        auto recv_task = asio::co_spawn(
            SrtReactor::get_instance().get_io_context(),
            [recv_socket = std::move(recv_socket)]() mutable -> asio::awaitable<void> {
                try {
                    char buffer[1024];
                    uint32_t frames = 0;
                    
                    while (recv_socket.is_connected()) {
                        size_t received = co_await recv_socket.async_receive(
                            asio::buffer(buffer));
                        
                        frames++;
                        if (frames % 50 == 0) {  // 每秒打印一次
                            std::cout << "已接收 " << frames << " 音频帧" << std::endl;
                        }
                    }
                } catch (const std::exception& e) {
                    std::cout << "接收错误: " << e.what() << std::endl;
                }
            },
            asio::use_future
        );
        
        // 等待任务完成
        send_task.get();
        recv_task.get();
        
    } catch (const std::exception& e) {
        std::cout << "通话错误: " << e.what() << std::endl;
    }
}

// ========================================
// 示例 4：多路复用广播服务器
// ========================================

asio::awaitable<void> broadcast_server(uint16_t port) {
    std::cout << "\n=== 启动广播服务器 ===" << std::endl;
    
    try {
        SrtAcceptor acceptor;
        acceptor.bind("0.0.0.0", port);
        
        // 客户端列表
        std::vector<std::shared_ptr<SrtSocket>> clients;
        std::mutex clients_mutex;
        
        // 广播源任务
        asio::co_spawn(
            SrtReactor::get_instance().get_io_context(),
            [&clients, &clients_mutex]() -> asio::awaitable<void> {
                asio::steady_timer timer(co_await asio::this_coro::executor);
                uint32_t seq = 0;
                
                while (true) {
                    timer.expires_after(100ms);  // 每100ms广播一次
                    co_await timer.async_wait();
                    
                    // 创建广播消息
                    std::string message = "广播消息 #" + std::to_string(seq++);
                    
                    // 发送给所有客户端
                    std::lock_guard<std::mutex> lock(clients_mutex);
                    auto it = clients.begin();
                    while (it != clients.end()) {
                        try {
                            if ((*it)->is_connected()) {
                                // 发送时不使用co_await，避免阻塞其他客户端
                                (*it)->send(asio::buffer(message));
                                ++it;
                            } else {
                                it = clients.erase(it);
                            }
                        } catch (...) {
                            it = clients.erase(it);
                        }
                    }
                    
                    if (!clients.empty() && seq % 10 == 0) {
                        std::cout << "广播消息到 " << clients.size() 
                                 << " 个客户端" << std::endl;
                    }
                }
            },
            asio::detached
        );
        
        // 接受客户端连接
        while (true) {
            auto client = std::make_shared<SrtSocket>(
                co_await acceptor.async_accept());
            
            auto [addr, port] = client->get_peer_address();
            std::cout << "新客户端加入广播: " << addr << ":" << port << std::endl;
            
            // 添加到客户端列表
            {
                std::lock_guard<std::mutex> lock(clients_mutex);
                clients.push_back(client);
            }
            
            // 为每个客户端启动心跳检测
            asio::co_spawn(
                SrtReactor::get_instance().get_io_context(),
                [client, &clients, &clients_mutex]() -> asio::awaitable<void> {
                    try {
                        char buffer[1024];
                        while (client->is_connected()) {
                            // 接收客户端消息（如心跳）
                            co_await client->async_receive(asio::buffer(buffer));
                        }
                    } catch (...) {
                        // 客户端断开
                    }
                    
                    // 从列表中移除
                    std::lock_guard<std::mutex> lock(clients_mutex);
                    clients.erase(
                        std::remove(clients.begin(), clients.end(), client),
                        clients.end()
                    );
                    std::cout << "客户端离开广播" << std::endl;
                },
                asio::detached
            );
        }
    } catch (const std::exception& e) {
        std::cout << "广播服务器错误: " << e.what() << std::endl;
    }
}

// ========================================
// 主函数
// ========================================

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "用法：" << std::endl;
        std::cout << "  视频流服务器: " << argv[0] << " server <port>" << std::endl;
        std::cout << "  视频流客户端: " << argv[0] << " client <server> <port> <stream_id>" << std::endl;
        std::cout << "  双向通话主叫: " << argv[0] << " call-caller <local_port> <remote_addr> <remote_port>" << std::endl;
        std::cout << "  双向通话被叫: " << argv[0] << " call-callee <local_port> <remote_addr> <remote_port>" << std::endl;
        std::cout << "  广播服务器: " << argv[0] << " broadcast <port>" << std::endl;
        return 1;
    }
    
    std::string mode = argv[1];
    
    try {
        auto& reactor = SrtReactor::get_instance();
        
        if (mode == "server" && argc >= 3) {
            uint16_t port = std::stoi(argv[2]);
            asio::co_spawn(
                reactor.get_io_context(),
                video_stream_server(port),
                asio::detached
            );
        }
        else if (mode == "client" && argc >= 5) {
            std::string server = argv[2];
            uint16_t port = std::stoi(argv[3]);
            std::string stream_id = argv[4];
            asio::co_spawn(
                reactor.get_io_context(),
                video_stream_client(server, port, stream_id),
                asio::detached
            );
        }
        else if (mode == "call-caller" && argc >= 5) {
            uint16_t local_port = std::stoi(argv[2]);
            std::string remote_addr = argv[3];
            uint16_t remote_port = std::stoi(argv[4]);
            asio::co_spawn(
                reactor.get_io_context(),
                bidirectional_call_peer(local_port, remote_addr, remote_port, true),
                asio::detached
            );
        }
        else if (mode == "call-callee" && argc >= 5) {
            uint16_t local_port = std::stoi(argv[2]);
            std::string remote_addr = argv[3];
            uint16_t remote_port = std::stoi(argv[4]);
            asio::co_spawn(
                reactor.get_io_context(),
                bidirectional_call_peer(local_port, remote_addr, remote_port, false),
                asio::detached
            );
        }
        else if (mode == "broadcast" && argc >= 3) {
            uint16_t port = std::stoi(argv[2]);
            asio::co_spawn(
                reactor.get_io_context(),
                broadcast_server(port),
                asio::detached
            );
        }
        else {
            std::cerr << "参数错误" << std::endl;
            return 1;
        }
        
        // 运行事件循环
        reactor.get_io_context().run();
        
    } catch (const std::exception& e) {
        std::cerr << "程序错误: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
