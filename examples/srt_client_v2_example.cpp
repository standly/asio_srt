// srt_client_v2_example.cpp - 增强的 SRT 客户端示例
// 演示新的连接回调和选项管理功能

#include <asrt/srt_socket.hpp>
#include <asrt/srt_reactor.hpp>
#include <asrt/srt_log.hpp>
#include <iostream>
#include <string>
#include <cstring>
#include <map>

using namespace asrt;

// 客户端主逻辑
asio::awaitable<void> run_client(const std::string& host, uint16_t port, 
                                 const std::string& stream_id, 
                                 const std::string& profile) {
    try {
        auto& reactor = SrtReactor::get_instance();
        
        // 根据 profile 设置不同的选项
        std::map<std::string, std::string> socket_options;
        
        // 基础选项（所有 profile 共享）
        socket_options["messageapi"] = "1";      // 消息模式
        socket_options["nakreport"] = "1";       // 启用 NAK 报告
        socket_options["conntimeo"] = "5000";    // 5 秒连接超时
        
        if (profile == "low_latency") {
            std::cout << "Using LOW LATENCY profile" << std::endl;
            socket_options["latency"] = "50";
            socket_options["sndbuf"] = "4194304";     // 4MB
            socket_options["rcvbuf"] = "4194304";     // 4MB
            socket_options["payloadsize"] = "1316";
            socket_options["maxbw"] = "0";
            socket_options["inputbw"] = "10000000";   // 10 Mbps
            socket_options["oheadbw"] = "50";         // 50% 开销（保守）
        } else if (profile == "high_throughput") {
            std::cout << "Using HIGH THROUGHPUT profile" << std::endl;
            socket_options["latency"] = "500";
            socket_options["sndbuf"] = "12582912";    // 12MB
            socket_options["rcvbuf"] = "12582912";    // 12MB
            socket_options["fc"] = "32768";
            socket_options["payloadsize"] = "1456";
            socket_options["maxbw"] = "-1";           // 无限制
        } else {
            std::cout << "Using DEFAULT profile" << std::endl;
            socket_options["latency"] = "120";
            socket_options["sndbuf"] = "8388608";     // 8MB
            socket_options["rcvbuf"] = "8388608";     // 8MB
            socket_options["payloadsize"] = "1316";
        }
        
        // 设置 stream ID（如果提供）
        if (!stream_id.empty()) {
            socket_options["streamid"] = stream_id;
        }
        
        // 创建 socket 并应用选项
        std::cout << "\nCreating socket with pre-configured options..." << std::endl;
        SrtSocket socket(socket_options, reactor);
        
        // 设置连接回调
        std::cout << "Setting up connect callback..." << std::endl;
        socket.set_connect_callback([](const std::error_code& ec, SrtSocket& sock) {
            std::cout << "\n========== Connect Callback ==========" << std::endl;
            
            if (ec) {
                std::cerr << "Connection FAILED: " << ec.message() << std::endl;
                std::cout << "Error category: " << ec.category().name() << std::endl;
                std::cout << "Error value: " << ec.value() << std::endl;
            } else {
                std::cout << "Connection SUCCESSFUL!" << std::endl;
                std::cout << "Local address: " << sock.local_address() << std::endl;
                std::cout << "Remote address: " << sock.remote_address() << std::endl;
                
                // 获取连接后的状态
                SRT_SOCKSTATUS status = sock.status();
                std::cout << "Socket status: " << status << " (SRTS_CONNECTED=" 
                         << SRTS_CONNECTED << ")" << std::endl;
                
                // 获取初始统计信息
                SRT_TRACEBSTATS stats;
                if (sock.get_stats(stats)) {
                    std::cout << "Initial RTT: " << stats.msRTT << " ms" << std::endl;
                }
            }
            
            std::cout << "======================================\n" << std::endl;
        });
        
        // 连接到服务器
        std::cout << "\nConnecting to " << host << ":" << port << "..." << std::endl;
        co_await socket.async_connect(host, port, std::chrono::milliseconds(5000));
        
        std::cout << "Connected! Starting data transmission..." << std::endl;
        std::cout << std::endl;
        
        // 发送测试数据
        for (int i = 0; i < 10; ++i) {
            // 构造消息
            std::string message = "Message #" + std::to_string(i + 1) + 
                                 " from " + profile + " client";
            if (!stream_id.empty()) {
                message += " (stream: " + stream_id + ")";
            }
            
            std::cout << "[" << i + 1 << "/10] Sending: " << message << std::endl;
            
            // 发送消息
            size_t sent = co_await socket.async_write_packet(
                message.c_str(), 
                message.size(),
                std::chrono::milliseconds(3000)
            );
            
            std::cout << "  Sent " << sent << " bytes" << std::endl;
            
            // 接收回显
            char buffer[2048];
            size_t received = co_await socket.async_read_packet(
                buffer, 
                sizeof(buffer),
                std::chrono::milliseconds(5000)
            );
            
            std::cout << "  Received " << received << " bytes: " 
                     << std::string(buffer, received) << std::endl;
            
            // 每 5 个消息显示一次统计
            if ((i + 1) % 5 == 0) {
                SRT_TRACEBSTATS stats;
                if (socket.get_stats(stats)) {
                    std::cout << "\n  === Statistics ===" << std::endl;
                    std::cout << "  Packets sent: " << stats.pktSent << std::endl;
                    std::cout << "  Packets received: " << stats.pktRecv << std::endl;
                    std::cout << "  Send loss: " << stats.pktSndLoss << std::endl;
                    std::cout << "  Retransmitted: " << stats.pktRetrans << std::endl;
                    std::cout << "  RTT: " << stats.msRTT << " ms" << std::endl;
                    std::cout << "  Send rate: " << (stats.mbpsSendRate * 1000000 / 8) 
                             << " bytes/s" << std::endl;
                    std::cout << "  ==================\n" << std::endl;
                }
            }
            
            // 短暂延迟
            co_await asio::steady_timer(reactor.get_io_context(), std::chrono::milliseconds(500))
                .async_wait(asio::use_awaitable);
        }
        
        // 最终统计
        std::cout << "\n=== Final Statistics ===" << std::endl;
        SRT_TRACEBSTATS stats;
        if (socket.get_stats(stats)) {
            std::cout << "Total packets sent: " << stats.pktSent << std::endl;
            std::cout << "Total packets received: " << stats.pktRecv << std::endl;
            std::cout << "Total packets lost: " << stats.pktSndLoss << std::endl;
            std::cout << "Total retransmitted: " << stats.pktRetrans << std::endl;
            std::cout << "Average RTT: " << stats.msRTT << " ms" << std::endl;
            std::cout << "Bandwidth: " << (stats.mbpsSendRate * 1000000 / 8) << " bytes/s" << std::endl;
            
            if (stats.pktSent > 0) {
                double loss_rate = (double)stats.pktSndLoss / stats.pktSent * 100.0;
                std::cout << "Loss rate: " << loss_rate << "%" << std::endl;
            }
        }
        
        std::cout << "\nClient finished successfully!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "\n[Client] Error: " << e.what() << std::endl;
    }
}

// 显示使用说明
void show_usage(const char* program) {
    std::cout << "Usage: " << program << " [options] <host> <port>" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -s <streamid>    Set stream ID" << std::endl;
    std::cout << "  -p <profile>     Set connection profile:" << std::endl;
    std::cout << "                   low_latency, high_throughput, default" << std::endl;
    std::cout << "  -h               Show this help" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  " << program << " 127.0.0.1 9000" << std::endl;
    std::cout << "  " << program << " -p low_latency 127.0.0.1 9000" << std::endl;
    std::cout << "  " << program << " -s mystream -p high_throughput 192.168.1.100 9000" << std::endl;
}

int main(int argc, char* argv[]) {
    try {
        // 默认值
        std::string host = "127.0.0.1";
        uint16_t port = 9000;
        std::string stream_id;
        std::string profile = "default";
        
        // 解析命令行参数
        int opt_index = 1;
        while (opt_index < argc && argv[opt_index][0] == '-') {
            std::string opt = argv[opt_index];
            if (opt == "-s" && opt_index + 1 < argc) {
                stream_id = argv[++opt_index];
            } else if (opt == "-p" && opt_index + 1 < argc) {
                profile = argv[++opt_index];
            } else if (opt == "-h" || opt == "--help") {
                show_usage(argv[0]);
                return 0;
            } else {
                std::cerr << "Unknown option: " << opt << std::endl;
                show_usage(argv[0]);
                return 1;
            }
            opt_index++;
        }
        
        // 获取 host 和 port
        if (opt_index < argc) {
            host = argv[opt_index++];
        }
        if (opt_index < argc) {
            port = static_cast<uint16_t>(std::stoi(argv[opt_index]));
        }
        
        std::cout << "╔══════════════════════════════════════╗" << std::endl;
        std::cout << "║      SRT Client V2 Example           ║" << std::endl;
        std::cout << "╚══════════════════════════════════════╝" << std::endl;
        std::cout << "Server: " << host << ":" << port << std::endl;
        std::cout << "Profile: " << profile << std::endl;
        if (!stream_id.empty()) {
            std::cout << "Stream ID: " << stream_id << std::endl;
        }
        std::cout << std::endl;
        
        // 设置日志级别
        SrtReactor::set_log_level(LogLevel::Debug);
        
        // 自定义日志格式（带颜色）
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
        
        // 获取 reactor 实例并启动客户端
        auto& reactor = SrtReactor::get_instance();
        
        // 启动客户端协程
        asio::co_spawn(
            reactor.get_io_context(),
            run_client(host, port, stream_id, profile),
            [](std::exception_ptr e) {
                if (e) {
                    try {
                        std::rethrow_exception(e);
                    } catch (const std::exception& ex) {
                        std::cerr << "[Main] Client coroutine exception: " << ex.what() << std::endl;
                    }
                }
            }
        );
        
        // 运行一段时间
        std::this_thread::sleep_for(std::chrono::seconds(30));
        
    } catch (const std::exception& e) {
        std::cerr << "[Main] Fatal error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
