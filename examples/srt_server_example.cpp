// srt_server_example.cpp - SRT 服务器示例
// 演示如何使用 SrtAcceptor 监听和接受连接，以及如何读写数据包

#include <asrt/srt_acceptor.hpp>
#include <asrt/srt_socket.hpp>
#include <asrt/srt_reactor.hpp>
#include <asrt/srt_log.hpp>
#include <iostream>
#include <memory>

using namespace asrt;

// 处理客户端连接的协程
asio::awaitable<void> handle_client(SrtSocket client) {
    try {
        std::cout << "Client connected from: " << client.remote_address() << std::endl;
        
        // 读取和回显数据包
        char buffer[2048];
        
        while (client.is_open()) {
            try {
                // 读取一个数据包（带超时）
                size_t bytes = co_await client.async_read_packet(
                    buffer, sizeof(buffer), 
                    std::chrono::milliseconds(5000)
                );
                
                std::cout << "Received " << bytes << " bytes from client" << std::endl;
                
                // 回显数据包
                size_t sent = co_await client.async_write_packet(buffer, bytes);
                
                std::cout << "Echoed " << sent << " bytes back to client" << std::endl;
                
            } catch (const asio::system_error& e) {
                if (e.code() == std::errc::timed_out) {
                    std::cout << "Read timeout, checking if client is still connected..." << std::endl;
                    
                    // 检查连接状态
                    if (client.status() != SRTS_CONNECTED) {
                        std::cout << "Client disconnected" << std::endl;
                        break;
                    }
                } else {
                    std::cerr << "Error: " << e.what() << std::endl;
                    break;
                }
            }
        }
        
        std::cout << "Client handler finished" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Exception in client handler: " << e.what() << std::endl;
    }
}

// 服务器主循环
asio::awaitable<void> run_server(uint16_t port) {
    try {
        // 获取 reactor 实例
        auto& reactor = SrtReactor::get_instance();
        
        // 创建 acceptor
        SrtAcceptor acceptor(reactor);
        
        // 设置 SRT 选项
        std::map<std::string, std::string> options = {
            {"latency", "200"},           // 200ms 延迟
            {"rcvbuf", "8388608"},        // 8MB 接收缓冲区
            {"messageapi", "1"},          // 消息模式
            {"payloadsize", "1316"}       // 负载大小
        };
        
        if (!acceptor.set_options(options)) {
            std::cerr << "Warning: Some options failed to set" << std::endl;
        }
        
        // 设置监听回调（可选）
        acceptor.set_listener_callback([](const std::error_code& ec, SrtSocket /*client*/) {
            if (ec) {
                std::cerr << "Listener callback error: " << ec.message() << std::endl;
            } else {
                std::cout << "New connection accepted in callback!" << std::endl;
            }
        });
        
        // 绑定并监听
        acceptor.bind(port);
        
        std::cout << "Server listening on: " << acceptor.local_address() << std::endl;
        std::cout << "Waiting for connections..." << std::endl;
        
        // 接受连接循环
        while (true) {
            try {
                // 接受一个连接
                SrtSocket client = co_await acceptor.async_accept();
                
                std::cout << "Accepted new connection, spawning handler..." << std::endl;
                
                // 为每个客户端启动一个协程
                asio::co_spawn(
                    reactor.get_io_context(),
                    handle_client(std::move(client)),
                    asio::detached
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
        
        std::cout << "=== SRT Server Example ===" << std::endl;
        std::cout << "Port: " << port << std::endl;
        std::cout << std::endl;
        
        // 设置日志级别
        SrtReactor::set_log_level(LogLevel::Debug);
        
        // 可选：设置自定义日志回调
        SrtReactor::set_log_callback([](LogLevel level, const char* area, const char* message) {
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
        std::cout << "Press Ctrl+C to stop the server..." << std::endl;
        std::cout << std::endl;
        
        // 简单的阻塞等待
        std::this_thread::sleep_for(std::chrono::hours(24));
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}

