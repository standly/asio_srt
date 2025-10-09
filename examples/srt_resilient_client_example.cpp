// srt_resilient_client_example.cpp - 带有错误处理和重连机制的客户端示例

#include "asrt/srt_reactor.hpp"
#include "asrt/srt_socket.hpp"
#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/steady_timer.hpp>
#include <iostream>
#include <atomic>
#include <chrono>
#include <queue>
#include <mutex>

using namespace asrt;
using namespace std::chrono_literals;

// 连接状态
enum class ConnectionState {
    Disconnected,
    Connecting,
    Connected,
    Reconnecting
};

// 重连配置
struct ReconnectConfig {
    std::chrono::milliseconds initial_delay = 1s;
    std::chrono::milliseconds max_delay = 30s;
    double backoff_multiplier = 2.0;
    int max_attempts = -1;  // -1 表示无限重试
};

// 可靠的SRT客户端类
class ResilientSrtClient {
public:
    ResilientSrtClient(const std::string& server_addr, uint16_t server_port)
        : server_addr_(server_addr)
        , server_port_(server_port)
        , state_(ConnectionState::Disconnected)
        , reconnect_attempts_(0) {
    }
    
    // 设置重连配置
    void set_reconnect_config(const ReconnectConfig& config) {
        reconnect_config_ = config;
    }
    
    // 设置连接选项
    void set_socket_options(const std::map<std::string, std::string>& options) {
        socket_options_ = options;
    }
    
    // 启动客户端
    asio::awaitable<void> start() {
        while (true) {
            try {
                // 创建新的socket
                socket_ = std::make_unique<SrtSocket>();
                
                // 应用socket选项
                for (const auto& [key, value] : socket_options_) {
                    socket_->set_option(key, value);
                }
                
                // 更新状态
                state_ = ConnectionState::Connecting;
                std::cout << "正在连接到 " << server_addr_ << ":" << server_port_ << "..." << std::endl;
                
                // 尝试连接
                co_await socket_->async_connect(server_addr_, server_port_, 5s);
                
                // 连接成功
                state_ = ConnectionState::Connected;
                reconnect_attempts_ = 0;
                std::cout << "连接成功！" << std::endl;
                
                // 启动数据处理
                co_await handle_connection();
                
            } catch (const std::system_error& e) {
                std::cout << "连接错误: " << e.what() << std::endl;
                
                // 处理重连
                if (reconnect_config_.max_attempts < 0 || 
                    reconnect_attempts_ < reconnect_config_.max_attempts) {
                    
                    state_ = ConnectionState::Reconnecting;
                    reconnect_attempts_++;
                    
                    // 计算重连延迟（指数退避）
                    auto delay = std::min(
                        std::chrono::milliseconds(static_cast<int64_t>(
                            reconnect_config_.initial_delay.count() * 
                            std::pow(reconnect_config_.backoff_multiplier, reconnect_attempts_ - 1)
                        )),
                        reconnect_config_.max_delay
                    );
                    
                    std::cout << "将在 " << delay.count() << "ms 后重连（第 " 
                             << reconnect_attempts_ << " 次尝试）" << std::endl;
                    
                    // 等待重连
                    asio::steady_timer timer(co_await asio::this_coro::executor);
                    timer.expires_after(delay);
                    co_await timer.async_wait();
                    
                } else {
                    std::cout << "达到最大重连次数，停止重连" << std::endl;
                    break;
                }
            }
        }
    }
    
    // 发送数据（线程安全）
    void send_data(const std::string& data) {
        std::lock_guard<std::mutex> lock(send_queue_mutex_);
        send_queue_.push(data);
        send_cv_.notify_one();
    }
    
    // 获取连接状态
    ConnectionState get_state() const {
        return state_.load();
    }
    
    // 获取统计信息
    void print_statistics() {
        if (socket_ && socket_->is_connected()) {
            try {
                // 获取SRT统计信息
                auto rtt = socket_->get_option("SRTO_RTT");
                auto bandwidth = socket_->get_option("SRTO_BANDWIDTH");
                auto send_rate = socket_->get_option("SRTO_SENDRATE");
                auto recv_rate = socket_->get_option("SRTO_RECVRATE");
                
                std::cout << "\n=== SRT 统计信息 ===" << std::endl;
                std::cout << "RTT: " << rtt << " us" << std::endl;
                std::cout << "带宽: " << bandwidth << " bps" << std::endl;
                std::cout << "发送速率: " << send_rate << " bps" << std::endl;
                std::cout << "接收速率: " << recv_rate << " bps" << std::endl;
            } catch (const std::exception& e) {
                std::cout << "获取统计信息失败: " << e.what() << std::endl;
            }
        }
    }
    
private:
    // 处理已建立的连接
    asio::awaitable<void> handle_connection() {
        // 启动发送任务
        auto send_task = asio::co_spawn(
            SrtReactor::get_instance().get_io_context(),
            handle_send(),
            asio::use_future
        );
        
        // 启动接收任务
        auto recv_task = asio::co_spawn(
            SrtReactor::get_instance().get_io_context(),
            handle_receive(),
            asio::use_future
        );
        
        // 启动心跳任务
        auto heartbeat_task = asio::co_spawn(
            SrtReactor::get_instance().get_io_context(),
            handle_heartbeat(),
            asio::use_future
        );
        
        try {
            // 等待任意任务结束（通常是因为错误）
            auto [f1, f2, f3] = co_await asio::experimental::make_parallel_group(
                send_task.async_wait(asio::deferred),
                recv_task.async_wait(asio::deferred),
                heartbeat_task.async_wait(asio::deferred)
            ).async_wait(
                asio::experimental::wait_for_one(),
                asio::deferred
            );
            
            // 检查哪个任务结束了
            if (f1.has_value()) {
                send_task.get();  // 可能抛出异常
            } else if (f2.has_value()) {
                recv_task.get();
            } else if (f3.has_value()) {
                heartbeat_task.get();
            }
            
        } catch (const std::exception& e) {
            std::cout << "连接处理错误: " << e.what() << std::endl;
        }
        
        // 更新状态
        state_ = ConnectionState::Disconnected;
    }
    
    // 处理发送
    asio::awaitable<void> handle_send() {
        try {
            while (socket_->is_connected()) {
                // 等待发送数据
                std::unique_lock<std::mutex> lock(send_queue_mutex_);
                if (send_queue_.empty()) {
                    // 使用条件变量等待，但定期检查连接状态
                    send_cv_.wait_for(lock, 100ms);
                    continue;
                }
                
                // 获取待发送数据
                std::string data = send_queue_.front();
                send_queue_.pop();
                lock.unlock();
                
                // 发送数据
                size_t sent = co_await socket_->async_send(asio::buffer(data));
                total_sent_ += sent;
                
                std::cout << "发送数据: " << sent << " 字节" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cout << "发送错误: " << e.what() << std::endl;
            throw;
        }
    }
    
    // 处理接收
    asio::awaitable<void> handle_receive() {
        try {
            char buffer[1500];
            
            while (socket_->is_connected()) {
                // 接收数据
                size_t received = co_await socket_->async_receive(asio::buffer(buffer));
                total_received_ += received;
                
                // 处理接收到的数据
                std::cout << "接收数据: " << received << " 字节" << std::endl;
                
                // 这里可以添加数据处理逻辑
                on_data_received(buffer, received);
            }
        } catch (const std::exception& e) {
            std::cout << "接收错误: " << e.what() << std::endl;
            throw;
        }
    }
    
    // 处理心跳
    asio::awaitable<void> handle_heartbeat() {
        try {
            asio::steady_timer timer(co_await asio::this_coro::executor);
            
            while (socket_->is_connected()) {
                // 每5秒发送一次心跳
                timer.expires_after(5s);
                co_await timer.async_wait();
                
                // 发送心跳消息
                std::string heartbeat = "HEARTBEAT";
                send_data(heartbeat);
                
                // 打印统计信息
                print_statistics();
            }
        } catch (const std::exception& e) {
            std::cout << "心跳错误: " << e.what() << std::endl;
            throw;
        }
    }
    
    // 数据接收回调
    void on_data_received(const char* data, size_t size) {
        // 这里可以实现具体的数据处理逻辑
        // 例如：解析协议、处理业务逻辑等
        
        // 简单示例：检查是否是心跳响应
        if (size >= 9 && std::memcmp(data, "HEARTBEAT", 9) == 0) {
            std::cout << "收到心跳响应" << std::endl;
        }
    }
    
private:
    std::string server_addr_;
    uint16_t server_port_;
    std::unique_ptr<SrtSocket> socket_;
    std::map<std::string, std::string> socket_options_;
    
    std::atomic<ConnectionState> state_;
    ReconnectConfig reconnect_config_;
    int reconnect_attempts_;
    
    // 发送队列
    std::queue<std::string> send_queue_;
    std::mutex send_queue_mutex_;
    std::condition_variable send_cv_;
    
    // 统计
    std::atomic<uint64_t> total_sent_{0};
    std::atomic<uint64_t> total_received_{0};
};

// ========================================
// 示例：使用可靠客户端
// ========================================

asio::awaitable<void> demo_resilient_client() {
    // 创建客户端
    ResilientSrtClient client("127.0.0.1", 9000);
    
    // 配置重连策略
    ReconnectConfig reconnect_config;
    reconnect_config.initial_delay = 1s;
    reconnect_config.max_delay = 30s;
    reconnect_config.backoff_multiplier = 2.0;
    reconnect_config.max_attempts = -1;  // 无限重试
    client.set_reconnect_config(reconnect_config);
    
    // 配置socket选项
    std::map<std::string, std::string> socket_options = {
        {"SRTO_LATENCY", "200"},        // 200ms延迟
        {"SRTO_SNDBUF", "8192000"},     // 8MB发送缓冲
        {"SRTO_RCVBUF", "8192000"},     // 8MB接收缓冲
        {"SRTO_STREAMID", "resilient-client-demo"}
    };
    client.set_socket_options(socket_options);
    
    // 启动客户端（包含自动重连）
    co_await client.start();
}

// ========================================
// 主函数
// ========================================

int main() {
    std::cout << "=== SRT 可靠客户端示例 ===" << std::endl;
    std::cout << "功能特性：" << std::endl;
    std::cout << "- 自动重连（指数退避）" << std::endl;
    std::cout << "- 错误处理和恢复" << std::endl;
    std::cout << "- 心跳机制" << std::endl;
    std::cout << "- 实时统计信息" << std::endl;
    std::cout << "- 线程安全的数据发送" << std::endl;
    std::cout << std::endl;
    
    try {
        auto& reactor = SrtReactor::get_instance();
        
        // 设置日志级别
        SrtReactor::set_log_level(LogLevel::Notice);
        
        // 启动演示客户端
        asio::co_spawn(
            reactor.get_io_context(),
            demo_resilient_client(),
            asio::detached
        );
        
        // 模拟用户输入（在实际应用中，这可以是GUI或其他接口）
        asio::co_spawn(
            reactor.get_io_context(),
            []() -> asio::awaitable<void> {
                asio::steady_timer timer(co_await asio::this_coro::executor);
                
                // 等待客户端启动
                timer.expires_after(2s);
                co_await timer.async_wait();
                
                std::cout << "\n提示：客户端正在尝试连接到 127.0.0.1:9000" << std::endl;
                std::cout << "如果没有服务器运行，客户端将自动重试连接" << std::endl;
                std::cout << "你可以启动/停止服务器来测试重连功能" << std::endl;
                std::cout << "\n按 Ctrl+C 退出程序" << std::endl;
                
                co_return;
            },
            asio::detached
        );
        
        // 运行事件循环
        reactor.get_io_context().run();
        
    } catch (const std::exception& e) {
        std::cerr << "程序错误: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
