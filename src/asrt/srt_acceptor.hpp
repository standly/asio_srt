// srt_acceptor.hpp - SRT acceptor for listening and accepting connections
#pragma once

#include <asio.hpp>
#include <asio/awaitable.hpp>
#include <srt/srt.h>
#include <string>
#include <functional>
#include <memory>
#include <map>

#include "srt_reactor.hpp"
#include "srt_socket.hpp"
#include "srt_error.hpp"
#include "srt_log.hpp"
#include "srt_socket_options.hpp"

namespace asrt {

// SRT Acceptor 类
class SrtAcceptor {
public:
    using ListenerCallback = std::function<int(SrtSocket&, int hsversion, const std::string& streamid)>;
    
    // ✅ 构造函数（必须提供 Reactor）
    explicit SrtAcceptor(SrtReactor& reactor);
    
    // ✅ 带选项的构造函数
    SrtAcceptor(SrtReactor& reactor,
                const std::map<std::string, std::string>& options);
    
    // 移动语义
    SrtAcceptor(SrtAcceptor&& other) noexcept;
    SrtAcceptor& operator=(SrtAcceptor&& other) noexcept;
    
    // 禁用拷贝
    SrtAcceptor(const SrtAcceptor&) = delete;
    SrtAcceptor& operator=(const SrtAcceptor&) = delete;
    
    // 析构函数
    ~SrtAcceptor();
    
    // ========================================
    // 监听操作
    // ========================================
    
    // 绑定到本地地址并开始监听
    void bind(const std::string& address, uint16_t port, int backlog = 5);
    void bind(uint16_t port, int backlog = 5) {
        bind("0.0.0.0", port, backlog);
    }
    
    // ========================================
    // 接受连接
    // ========================================
    
    // 异步接受一个连接
    asio::awaitable<SrtSocket> async_accept();
    asio::awaitable<SrtSocket> async_accept(std::chrono::milliseconds timeout);
    
    // 设置监听回调（在接受到新连接时调用）
    // 回调返回 0 表示接受连接，-1 表示拒绝
    void set_listener_callback(ListenerCallback callback);
    
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
    // 状态管理
    // ========================================
    
    // 关闭 acceptor
    void close();
    
    // 检查是否正在监听
    bool is_open() const {
        return sock_ != SRT_INVALID_SOCK;
    }
    
    // 获取原始 socket 句柄
    SRTSOCKET native_handle() const {
        return sock_;
    }
    
    // 获取监听地址
    std::string local_address() const;
    
private:
    // 应用配置的选项（分阶段）
    bool apply_pre_bind_options();
    bool apply_pre_options();
    
    // 尝试直接接受（不等待）
    SRTSOCKET try_accept(sockaddr_storage& client_addr, int& addr_len, std::error_code& ec);
    
    // Socket 地址辅助函数
    static std::string sockaddr_to_string(const sockaddr* addr);
    
    // SRT 原生监听回调（静态）
    static int srt_listen_callback_fn(void* opaq, SRTSOCKET ns, int hsversion,
                                      const struct sockaddr* peeraddr, const char* streamid);
    
private:
    SrtReactor& reactor_;
    SRTSOCKET sock_;
    SrtSocketOptions options_;
    ListenerCallback listener_callback_;
    bool options_applied_pre_bind_ = false;
    bool options_applied_pre_ = false;
};

} // namespace asrt

