// srt_socket.hpp - SRT socket wrapper with async I/O support
#pragma once

#include <asio.hpp>
#include <asio/awaitable.hpp>
#include <srt/srt.h>
#include <string>
#include <map>
#include <memory>
#include <functional>
#include <chrono>

#include "srt_reactor.hpp"
#include "srt_error.hpp"
#include "srt_log.hpp"
#include "srt_socket_options.hpp"

namespace asrt {

// 前向声明
class SrtSocket;
class SrtAcceptor;

// SRT Socket 类
class SrtSocket {
public:
    using ConnectCallback = std::function<void(const std::error_code&, SrtSocket&)>;
    
    // ✅ 构造函数（必须提供 Reactor）
    explicit SrtSocket(SrtReactor& reactor);
    
    // ✅ 带选项的构造函数
    SrtSocket(SrtReactor& reactor,
              const std::map<std::string, std::string>& options);
    
    // 移动语义
    SrtSocket(SrtSocket&& other) noexcept;
    SrtSocket& operator=(SrtSocket&& other) noexcept;
    
    // 禁用拷贝
    SrtSocket(const SrtSocket&) = delete;
    SrtSocket& operator=(const SrtSocket&) = delete;
    
    // 析构函数
    ~SrtSocket();
    
    // ========================================
    // 连接操作
    // ========================================
    
    // 异步连接到远程地址
    asio::awaitable<void> async_connect(const std::string& host, uint16_t port);
    asio::awaitable<void> async_connect(const std::string& host, uint16_t port, 
                                         std::chrono::milliseconds timeout);
    
    // 设置连接回调（在连接成功或失败时调用）
    void set_connect_callback(ConnectCallback callback) {
        connect_callback_ = std::move(callback);
    }
    
    // ========================================
    // 选项设置
    // ========================================
    
    // 设置单个选项（字符串格式）
    bool set_option(const std::string& option_str) {
        return options_.set_option(option_str);
    }
    
    // 批量设置选项
    bool set_options(const std::map<std::string, std::string>& options) {
        return options_.set_options(options);
    }
    
    
    // ========================================
    // 基于 Packet 的读写接口
    // ========================================
    
    // 异步读取一个数据包
    // 返回读取的字节数
    asio::awaitable<size_t> async_read_packet(char* data, size_t max_size);
    asio::awaitable<size_t> async_read_packet(char* data, size_t max_size, 
                                               std::chrono::milliseconds timeout);
    
    // 异步写入一个数据包
    // 返回写入的字节数
    asio::awaitable<size_t> async_write_packet(const char* data, size_t size);
    asio::awaitable<size_t> async_write_packet(const char* data, size_t size, 
                                                std::chrono::milliseconds timeout);
    
    // ========================================
    // Socket 状态和管理
    // ========================================
    
    // 关闭 socket
    void close();
    
    // 检查是否打开
    bool is_open() const {
        return sock_ != SRT_INVALID_SOCK;
    }
    
    // 获取原始 socket 句柄
    SRTSOCKET native_handle() const {
        return sock_;
    }
    
    // 获取连接状态
    SRT_SOCKSTATUS status() const;
    
    // 获取统计信息
    bool get_stats(SRT_TRACEBSTATS& stats) const;
    
    // 获取本地和远程地址
    std::string local_address() const;
    std::string remote_address() const;
    
private:
    // 从已接受的 socket 创建（给 SrtAcceptor 使用）
    explicit SrtSocket(SRTSOCKET sock, SrtReactor& reactor);
    friend class SrtAcceptor;
    
    // 应用配置的选项（分阶段）
    bool apply_pre_bind_options();
    bool apply_pre_options();
    bool apply_post_options();
    
    // 尝试直接读取（不等待）
    int try_recv_packet(char* data, size_t max_size, std::error_code& ec);
    
    // 尝试直接写入（不等待）
    int try_send_packet(const char* data, size_t size, std::error_code& ec);
    
    // Socket 地址辅助函数
    static std::string sockaddr_to_string(const sockaddr* addr);
    
    // SRT 原生连接回调（静态）
    static void srt_connect_callback_fn(void* opaq, SRTSOCKET ns, int errorcode,
                                        const struct sockaddr* peeraddr, int token);
    
private:
    SrtReactor& reactor_;
    SRTSOCKET sock_;
    SrtSocketOptions options_;
    ConnectCallback connect_callback_;
    bool options_applied_pre_bind_ = false;
    bool options_applied_pre_ = false;
};

} // namespace asrt

