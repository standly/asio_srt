// srt_advanced_server.cpp - 高级 SRT 服务器示例
// 演示选项设置时机、回调使用和访问控制

#include <asrt/srt_acceptor.hpp>
#include <asrt/srt_socket.hpp>
#include <asrt/srt_reactor.hpp>
#include <asrt/srt_log.hpp>
#include <iostream>
#include <memory>
#include <map>

using namespace asrt;

// 客户端会话管理
struct ClientSession {
    std::string peer_address;
    std::chrono::steady_clock::time_point connect_time;
    size_t bytes_received = 0;
    size_t bytes_sent = 0;
};

std::map<SRTSOCKET, ClientSession> active_sessions;

// 处理客户端连接的协程
asio::awaitable<void> handle_client(SrtSocket client) {
    SRTSOCKET sock_id = client.native_handle();
    auto& session = active_sessions[sock_id];
    
    try {
        std::cout << "\n=== Client Connected ===" << std::endl;
        std::cout << "Peer: " << client.remote_address() << std::endl;
        std::cout << "Local: " << client.local_address() << std::endl;
        
        // 获取连接统计信息
        SRT_TRACEBSTATS stats;
        if (client.get_stats(stats)) {
            std::cout << "Initial RTT: " << stats.msRTT << " ms" << std::endl;
        }
        
        // 连接后可以设置 post 选项
        client.set_options({
            {"maxbw", "10000000"},    // 限制带宽为 10 Mbps
            {"rcvtimeo", "10000"}     // 10秒读超时
        });
        std::cout << "Applied post-connection options" << std::endl;
        
        // 读取和回显数据包
        char buffer[2048];
        int packet_count = 0;
        
        while (client.is_open()) {
            try {
                // 读取一个数据包
                size_t bytes = co_await client.async_read_packet(
                    buffer, sizeof(buffer), 
                    std::chrono::milliseconds(30000)  // 30秒超时
                );
                
                session.bytes_received += bytes;
                packet_count++;
                
                std::cout << "Packet #" << packet_count << ": " << bytes << " bytes from " 
                         << session.peer_address << std::endl;
                
                // 回显数据包
                size_t sent = co_await client.async_write_packet(buffer, bytes);
                session.bytes_sent += sent;
                
                // 每 10 个包输出一次统计
                if (packet_count % 10 == 0) {
                    if (client.get_stats(stats)) {
                        std::cout << "Stats - Packets: sent=" << stats.pktSent 
                                 << ", recv=" << stats.pktRecv
                                 << ", loss=" << stats.pktSndLoss
                                 << ", RTT=" << stats.msRTT << "ms"
                                 << ", BW=" << (stats.mbpsSendRate) << "Mbps"
                                 << std::endl;
                    }
                }
                
            } catch (const asio::system_error& e) {
                if (e.code() == std::errc::timed_out) {
                    std::cout << "Read timeout for " << session.peer_address 
                             << ", checking connection..." << std::endl;
                    
                    // 检查连接状态
                    if (client.status() != SRTS_CONNECTED) {
                        std::cout << "Client " << session.peer_address 
                                 << " disconnected" << std::endl;
                        break;
                    }
                    
                    // 超时但连接仍然有效，继续
                    continue;
                } else {
                    std::cerr << "Error reading from " << session.peer_address 
                             << ": " << e.what() << std::endl;
                    break;
                }
            }
        }
        
        // 输出会话统计
        auto duration = std::chrono::steady_clock::now() - session.connect_time;
        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
        
        std::cout << "\n=== Client Disconnected ===" << std::endl;
        std::cout << "Peer: " << session.peer_address << std::endl;
        std::cout << "Duration: " << seconds << " seconds" << std::endl;
        std::cout << "Packets processed: " << packet_count << std::endl;
        std::cout << "Bytes received: " << session.bytes_received << std::endl;
        std::cout << "Bytes sent: " << session.bytes_sent << std::endl;
        
        // 获取最终统计
        if (client.get_stats(stats)) {
            std::cout << "Final stats:" << std::endl;
            std::cout << "  Packets sent: " << stats.pktSent << std::endl;
            std::cout << "  Packets received: " << stats.pktRecv << std::endl;
            std::cout << "  Packets lost: " << stats.pktSndLoss << std::endl;
            std::cout << "  Packets retransmitted: " << stats.pktRetrans << std::endl;
            std::cout << "  Average RTT: " << stats.msRTT << " ms" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Exception in client handler for " << session.peer_address 
                 << ": " << e.what() << std::endl;
    }
    
    // 清理会话
    active_sessions.erase(sock_id);
}

// 服务器主循环
asio::awaitable<void> run_server(uint16_t port) {
    try {
        auto& reactor = SrtReactor::get_instance();
        SrtAcceptor acceptor(reactor);
        
        std::cout << "=== Advanced SRT Server ===" << std::endl;
        std::cout << "Configuring server options..." << std::endl;
        
        // 设置 pre-bind 选项（必须在 bind 前设置）
        std::map<std::string, std::string> pre_bind_options = {
            {"mss", "1500"},              // 最大段大小
            {"rcvbuf", "16777216"},       // 16MB 接收缓冲区
            {"sndbuf", "16777216"},       // 16MB 发送缓冲区
            {"udp_rcvbuf", "16777216"},   // UDP 接收缓冲区
            {"udp_sndbuf", "8388608"}     // UDP 发送缓冲区
        };
        
        if (!acceptor.set_options(pre_bind_options)) {
            std::cerr << "Warning: Some pre-bind options failed to set" << std::endl;
        }
        
        // 设置 pre 选项（必须在连接前设置）
        std::map<std::string, std::string> pre_options = {
            {"latency", "200"},           // 200ms 延迟
            {"rcvlatency", "200"},        // 接收延迟
            {"peerlatency", "200"},       // 对端延迟
            {"messageapi", "1"},          // 消息模式
            {"payloadsize", "1316"},      // 负载大小
            {"fc", "32768"},              // 流控窗口
            {"conntimeo", "5000"},        // 5秒连接超时
            {"peeridletimeo", "10000"}    // 10秒空闲超时
        };
        
        if (!acceptor.set_options(pre_options)) {
            std::cerr << "Warning: Some pre options failed to set" << std::endl;
        }
        
        // 可选：设置加密
        // acceptor.set_options({
        //     {"passphrase", "mySecretPassword123"},
        //     {"pbkeylen", "32"}  // AES-256
        // });
        
        // 设置监听回调
        acceptor.set_listener_callback([](SrtSocket& client, int hsversion, const std::string& streamid) -> int {
            SRTSOCKET sock_id = client.native_handle();
            std::string peer_addr = client.remote_address();
            
            std::cout << "\n>>> New connection request from " << peer_addr << std::endl;
            std::cout << "Handshake version: " << hsversion << std::endl;
            std::cout << "Stream ID: " << (streamid.empty() ? "(none)" : streamid) << std::endl;
            
            // 创建会话记录
            ClientSession session;
            session.peer_address = peer_addr;
            session.connect_time = std::chrono::steady_clock::now();
            active_sessions[sock_id] = session;
            
            // 可以在这里根据客户端信息设置不同的选项
            // 例如，根据 IP 地址限制带宽
            if (peer_addr.find("192.168.") == 0) {
                // 本地网络，不限制带宽
                std::cout << "Local network client, no bandwidth limit" << std::endl;
            } else {
                // 外部网络，限制带宽
                client.set_option("maxbw=5000000");  // 5 Mbps
                std::cout << "External client, bandwidth limited to 5 Mbps" << std::endl;
            }
            
            return 0;  // 接受连接
        });
        
        // 绑定并监听
        std::cout << "\nBinding to port " << port << "..." << std::endl;
        acceptor.bind(port, 10);  // backlog = 10
        
        std::cout << "Server listening on: " << acceptor.local_address() << std::endl;
        std::cout << "Options applied successfully" << std::endl;
        std::cout << "\nWaiting for connections..." << std::endl;
        std::cout << "Press Ctrl+C to stop\n" << std::endl;
        
        // 接受连接循环
        int connection_count = 0;
        while (true) {
            try {
                // 接受一个连接
                SrtSocket client = co_await acceptor.async_accept();
                connection_count++;
                
                std::cout << ">>> Accepted connection #" << connection_count 
                         << " from " << client.remote_address() << std::endl;
                
                // 为每个客户端启动一个协程
                asio::co_spawn(
                    reactor.get_io_context(),
                    handle_client(std::move(client)),
                    [peer = client.remote_address()](std::exception_ptr e) {
                        if (e) {
                            try {
                                std::rethrow_exception(e);
                            } catch (const std::exception& ex) {
                                std::cerr << "Client handler exception for " << peer 
                                         << ": " << ex.what() << std::endl;
                            }
                        }
                    }
                );
                
            } catch (const std::exception& e) {
                std::cerr << "Error accepting connection: " << e.what() << std::endl;
                // 继续接受其他连接
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Server error: " << e.what() << std::endl;
    }
}

int main(int argc, char* argv[]) {
    try {
        // 解析命令行参数
        uint16_t port = 9000;
        if (argc > 1) {
            port = static_cast<uint16_t>(std::stoi(argv[1]));
        }
        
        std::cout << "=== Advanced SRT Server Example ===" << std::endl;
        std::cout << "Port: " << port << std::endl;
        std::cout << "Features:" << std::endl;
        std::cout << "  - Pre-bind and pre options configuration" << std::endl;
        std::cout << "  - Post-connection options adjustment" << std::endl;
        std::cout << "  - Per-client bandwidth control" << std::endl;
        std::cout << "  - Detailed statistics tracking" << std::endl;
        std::cout << "  - Connection monitoring" << std::endl;
        std::cout << std::endl;
        
        // 设置日志级别
        SrtReactor::set_log_level(LogLevel::Notice);
        
        // 设置自定义日志回调
        SrtReactor::set_log_callback([](LogLevel level, const char* area, const char* message,
                                       const char* file, const char* function, int line) {
            const char* level_str = "";
            switch (level) {
                case LogLevel::Debug:    level_str = "[DEBUG]"; break;
                case LogLevel::Notice:   level_str = "[INFO ]"; break;
                case LogLevel::Warning:  level_str = "[WARN ]"; break;
                case LogLevel::Error:    level_str = "[ERROR]"; break;
                case LogLevel::Critical: level_str = "[FATAL]"; break;
            }
            
            // 只输出重要的日志
            if (level >= LogLevel::Notice) {
                std::cout << level_str << " [" << area << "] " << message << std::endl;
            }
        });
        
        // 获取 reactor 实例并启动服务器
        auto& reactor = SrtReactor::get_instance();
        
        // 启动服务器协程
        asio::co_spawn(
            reactor.get_io_context(),
            run_server(port),
            [](std::exception_ptr e) {
                if (e) {
                    try {
                        std::rethrow_exception(e);
                    } catch (const std::exception& ex) {
                        std::cerr << "Server coroutine exception: " << ex.what() << std::endl;
                    }
                }
            }
        );
        
        // 等待用户中断
        std::this_thread::sleep_for(std::chrono::hours(24));
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}

