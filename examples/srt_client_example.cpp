// srt_client_example.cpp - SRT 客户端示例
// 演示如何使用 SrtSocket 连接到服务器并发送/接收数据包

#include <asrt/srt_socket.hpp>
#include <asrt/srt_reactor.hpp>
#include <asrt/srt_log.hpp>
#include <iostream>
#include <string>
#include <cstring>

using namespace asrt;

// 客户端主逻辑
asio::awaitable<void> run_client(const std::string& host, uint16_t port) {
    try {
        // 获取 reactor 实例
        auto& reactor = SrtReactor::get_instance();
        
        // 创建 socket
        SrtSocket socket(reactor);
        
        // 设置 SRT 选项
        std::map<std::string, std::string> options = {
            {"latency", "200"},           // 200ms 延迟
            {"sndbuf", "8388608"},        // 8MB 发送缓冲区
            {"messageapi", "1"},          // 消息模式
            {"payloadsize", "1316"}       // 负载大小
        };
        
        if (!socket.set_options(options)) {
            std::cerr << "Warning: Some options failed to set" << std::endl;
        }
        
        // 也可以单独设置选项
        socket.set_option("conntimeo=5000");  // 5秒连接超时
        
        // 设置连接回调（可选）
        socket.set_connect_callback([](const std::error_code& ec, SrtSocket& sock) {
            if (ec) {
                std::cerr << "Connect callback error: " << ec.message() << std::endl;
            } else {
                std::cout << "Connected in callback! Local: " << sock.local_address() 
                         << ", Remote: " << sock.remote_address() << std::endl;
            }
        });
        
        std::cout << "Connecting to " << host << ":" << port << "..." << std::endl;
        
        // 连接到服务器（带超时）
        co_await socket.async_connect(host, port, std::chrono::milliseconds(5000));
        
        std::cout << "Connected successfully!" << std::endl;
        std::cout << "Local address: " << socket.local_address() << std::endl;
        std::cout << "Remote address: " << socket.remote_address() << std::endl;
        std::cout << std::endl;
        
        // 发送和接收测试数据
        for (int i = 0; i < 5; ++i) {
            // 准备要发送的消息
            std::string message = "Hello from client, message #" + std::to_string(i + 1);
            
            std::cout << "Sending: " << message << std::endl;
            
            // 发送数据包
            size_t sent = co_await socket.async_write_packet(
                message.c_str(), 
                message.size(),
                std::chrono::milliseconds(3000)  // 3秒超时
            );
            
            std::cout << "Sent " << sent << " bytes" << std::endl;
            
            // 接收回显
            char buffer[2048];
            size_t received = co_await socket.async_read_packet(
                buffer, 
                sizeof(buffer),
                std::chrono::milliseconds(5000)  // 5秒超时
            );
            
            std::cout << "Received " << received << " bytes: " 
                     << std::string(buffer, received) << std::endl;
            std::cout << std::endl;
            
            // 等待一会儿再发送下一条
            co_await asio::steady_timer(reactor.get_io_context(), std::chrono::seconds(1))
                .async_wait(asio::use_awaitable);
        }
        
        // 获取统计信息
        SRT_TRACEBSTATS stats;
        if (socket.get_stats(stats)) {
            std::cout << "=== Connection Statistics ===" << std::endl;
            std::cout << "Packets sent: " << stats.pktSent << std::endl;
            std::cout << "Packets received: " << stats.pktRecv << std::endl;
            std::cout << "Packets lost: " << stats.pktSndLoss << std::endl;
            std::cout << "Packets retransmitted: " << stats.pktRetrans << std::endl;
            std::cout << "RTT: " << stats.msRTT << " ms" << std::endl;
            std::cout << "Bandwidth: " << (stats.mbpsSendRate * 1000000 / 8) << " bytes/s" << std::endl;
        }
        
        std::cout << std::endl << "Client finished successfully" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Client error: " << e.what() << std::endl;
    }
}

int main(int argc, char* argv[]) {
    try {
        // 解析命令行参数
        std::string host = "127.0.0.1";
        uint16_t port = 9000;
        
        if (argc > 1) {
            host = argv[1];
        }
        if (argc > 2) {
            port = static_cast<uint16_t>(std::stoi(argv[2]));
        }
        
        std::cout << "=== SRT Client Example ===" << std::endl;
        std::cout << "Server: " << host << ":" << port << std::endl;
        std::cout << std::endl;
        
        // 设置日志级别
        SrtReactor::set_log_level(LogLevel::Debug);
        
        // 可选：设置自定义日志回调
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
            std::cout << level_str << " [" << area << "] " << message << std::endl;
        });
        
        // 获取 reactor 实例并启动客户端
        auto& reactor = SrtReactor::get_instance();
        
        // 启动客户端协程
        asio::co_spawn(
            reactor.get_io_context(),
            run_client(host, port),
            [](std::exception_ptr e) {
                if (e) {
                    try {
                        std::rethrow_exception(e);
                    } catch (const std::exception& ex) {
                        std::cerr << "Client coroutine exception: " << ex.what() << std::endl;
                    }
                }
            }
        );
        
        // 等待完成
        std::this_thread::sleep_for(std::chrono::seconds(30));
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}

