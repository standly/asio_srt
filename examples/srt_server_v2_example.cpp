// srt_server_v2_example.cpp - 增强的 SRT 服务器示例
// 演示新的回调机制和选项管理功能

#include <asrt/srt_acceptor.hpp>
#include <asrt/srt_socket.hpp>
#include <asrt/srt_reactor.hpp>
#include <asrt/srt_log.hpp>
#include <iostream>
#include <memory>
#include <map>
#include <string>

using namespace asrt;

// 处理客户端连接的协程
asio::awaitable<void> handle_client(SrtSocket client) {
    try {
        std::cout << "\n[Client Handler] Started for " << client.remote_address() << std::endl;
        
        // 获取连接统计信息
        SRT_TRACEBSTATS stats;
        if (client.get_stats(stats)) {
            std::cout << "[Client Handler] Initial RTT: " << stats.msRTT << " ms" << std::endl;
        }
        
        // 读取和处理数据包
        char buffer[2048];
        int packet_count = 0;
        
        while (client.is_open()) {
            try {
                // 读取一个数据包（带超时）
                size_t bytes = co_await client.async_read_packet(
                    buffer, sizeof(buffer), 
                    std::chrono::milliseconds(5000)
                );
                
                packet_count++;
                std::cout << "[Client Handler] Packet #" << packet_count 
                         << ": " << bytes << " bytes" << std::endl;
                
                // 添加序号前缀后回显
                std::string response = "Echo #" + std::to_string(packet_count) + ": ";
                response.append(buffer, bytes);
                
                size_t sent = co_await client.async_write_packet(
                    response.c_str(), response.size()
                );
                
                std::cout << "[Client Handler] Echoed " << sent << " bytes" << std::endl;
                
                // 每 10 个包输出一次统计信息
                if (packet_count % 10 == 0 && client.get_stats(stats)) {
                    std::cout << "[Client Handler] Stats - RTT: " << stats.msRTT 
                             << "ms, Loss: " << stats.pktSndLoss 
                             << ", Retrans: " << stats.pktRetrans << std::endl;
                }
                
            } catch (const asio::system_error& e) {
                if (e.code() == std::errc::timed_out) {
                    std::cout << "[Client Handler] Read timeout, checking connection..." << std::endl;
                    
                    if (client.status() != SRTS_CONNECTED) {
                        std::cout << "[Client Handler] Client disconnected" << std::endl;
                        break;
                    }
                } else {
                    std::cerr << "[Client Handler] Error: " << e.what() << std::endl;
                    break;
                }
            }
        }
        
        std::cout << "[Client Handler] Finished. Total packets: " << packet_count << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "[Client Handler] Exception: " << e.what() << std::endl;
    }
}

// 服务器主循环
asio::awaitable<void> run_server(uint16_t port) {
    try {
        auto& reactor = SrtReactor::get_instance();
        
        // 创建 acceptor 时直接设置选项
        std::map<std::string, std::string> acceptor_options = {
            // Pre-bind 选项
            {"mss", "1500"},              // 最大段大小
            {"udp_rcvbuf", "12582912"},   // 12MB UDP 接收缓冲区
            {"udp_sndbuf", "12582912"},   // 12MB UDP 发送缓冲区
            
            // Pre 选项  
            {"latency", "120"},           // 120ms 延迟（适合互联网）
            {"rcvbuf", "8388608"},        // 8MB SRT 接收缓冲区
            {"sndbuf", "8388608"},        // 8MB SRT 发送缓冲区
            {"fc", "25600"},              // 流控窗口
            {"messageapi", "1"},          // 消息模式
            {"payloadsize", "1316"},      // 负载大小
            {"nakreport", "1"},           // 启用 NAK 报告
            {"conntimeo", "3000"},        // 3 秒连接超时
            {"peeridletimeo", "5000"},    // 5 秒对端空闲超时
            
            // Post 选项（这些会在后面应用）
            {"rcvsyn", "0"},              // 非阻塞接收
            {"sndsyn", "0"},              // 非阻塞发送
            {"maxbw", "0"},               // 使用相对带宽模式
            {"inputbw", "0"},             // 自动估算输入带宽
            {"oheadbw", "25"}             // 25% 的带宽开销
        };
        
        std::cout << "Creating acceptor with pre-configured options..." << std::endl;
        SrtAcceptor acceptor(acceptor_options, reactor);
        
        // 设置监听回调（新的回调签名）
        std::cout << "Setting up listener callback..." << std::endl;
        acceptor.set_listener_callback([](SrtSocket& client, int hsversion, const std::string& streamid) -> int {
            std::cout << "\n========== Listener Callback ==========" << std::endl;
            std::cout << "Peer address: " << client.remote_address() << std::endl;
            std::cout << "Handshake version: " << hsversion << std::endl;
            std::cout << "Stream ID: " << (streamid.empty() ? "(none)" : streamid) << std::endl;
            
            // 基于 stream ID 的访问控制
            if (!streamid.empty()) {
                // 拒绝包含 "reject" 的 stream ID
                if (streamid.find("reject") != std::string::npos) {
                    std::cout << "Access DENIED - blacklisted stream ID" << std::endl;
                    std::cout << "======================================\n" << std::endl;
                    return -1;
                }
                
                // 为特定客户端类型设置不同的选项
                if (streamid == "low_latency") {
                    client.set_option("rcvlatency=50");
                    client.set_option("snddropdelay=50");
                    std::cout << "Applied LOW LATENCY profile" << std::endl;
                } else if (streamid == "high_throughput") {
                    client.set_option("rcvlatency=500");
                    client.set_option("rcvbuf=12582912");
                    client.set_option("fc=32768");
                    std::cout << "Applied HIGH THROUGHPUT profile" << std::endl;
                } else if (streamid.find("secure") != std::string::npos) {
                    // 这里可以设置加密相关选项
                    std::cout << "Applied SECURE profile" << std::endl;
                }
            }
            
            // 可以基于对端地址进行额外控制
            std::string peer_addr = client.remote_address();
            if (peer_addr.find("127.0.0.1") != std::string::npos) {
                std::cout << "Local connection detected" << std::endl;
            }
            
            std::cout << "Access GRANTED" << std::endl;
            std::cout << "======================================\n" << std::endl;
            return 0;  // 接受连接
        });
        
        // 绑定并监听
        std::cout << "Binding to port " << port << "..." << std::endl;
        acceptor.bind(port, 10);  // backlog = 10
        
        std::cout << "\n=== SRT Server V2 Started ===" << std::endl;
        std::cout << "Listening on: " << acceptor.local_address() << std::endl;
        std::cout << "Features:" << std::endl;
        std::cout << "  - Automatic option timing management" << std::endl;
        std::cout << "  - Stream ID based access control" << std::endl;
        std::cout << "  - Per-client option profiles" << std::endl;
        std::cout << "  - Real-time statistics monitoring" << std::endl;
        std::cout << "\nWaiting for connections..." << std::endl;
        
        // 接受连接循环
        int connection_count = 0;
        while (true) {
            try {
                // 接受一个连接
                SrtSocket client = co_await acceptor.async_accept();
                
                connection_count++;
                std::cout << "\n[Main] Connection #" << connection_count 
                         << " established from " << client.remote_address() << std::endl;
                
                // 为每个客户端启动一个协程
                asio::co_spawn(
                    reactor.get_io_context(),
                    handle_client(std::move(client)),
                    asio::detached
                );
                
            } catch (const std::exception& e) {
                std::cerr << "[Main] Error accepting connection: " << e.what() << std::endl;
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "[Main] Server error: " << e.what() << std::endl;
    }
}

int main(int argc, char* argv[]) {
    try {
        // 解析命令行参数
        uint16_t port = 9000;
        if (argc > 1) {
            port = static_cast<uint16_t>(std::stoi(argv[1]));
        }
        
        std::cout << "╔══════════════════════════════════════╗" << std::endl;
        std::cout << "║      SRT Server V2 Example           ║" << std::endl;
        std::cout << "╚══════════════════════════════════════╝" << std::endl;
        std::cout << "Port: " << port << std::endl;
        std::cout << std::endl;
        
        // 设置详细的日志
        SrtReactor::set_log_level(LogLevel::Debug);
        
        // 自定义日志格式
        SrtReactor::set_log_callback([](LogLevel level, const char* area, const char* message) {
            const char* level_str = "";
            const char* color_start = "";
            const char* color_end = "\033[0m";
            
            switch (level) {
                case LogLevel::Debug:    
                    level_str = "[DEBUG]"; 
                    color_start = "\033[36m";  // Cyan
                    break;
                case LogLevel::Notice:   
                    level_str = "[INFO ]"; 
                    color_start = "\033[32m";  // Green
                    break;
                case LogLevel::Warning:  
                    level_str = "[WARN ]"; 
                    color_start = "\033[33m";  // Yellow
                    break;
                case LogLevel::Error:    
                    level_str = "[ERROR]"; 
                    color_start = "\033[31m";  // Red
                    break;
                case LogLevel::Critical: 
                    level_str = "[FATAL]"; 
                    color_start = "\033[35m";  // Magenta
                    break;
            }
            
            std::cout << color_start << level_str << " [" << area << "] " 
                     << message << color_end << std::endl;
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
                        std::cerr << "[Main] Server coroutine exception: " << ex.what() << std::endl;
                    }
                }
            }
        );
        
        // 运行事件循环
        std::cout << "\nPress Ctrl+C to stop the server..." << std::endl;
        std::cout << std::endl;
        
        // 阻塞运行
        std::this_thread::sleep_for(std::chrono::hours(24));
        
    } catch (const std::exception& e) {
        std::cerr << "[Main] Fatal error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
