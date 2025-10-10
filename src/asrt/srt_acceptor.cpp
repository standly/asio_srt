// srt_acceptor_v2.cpp - 更新的 SRT acceptor 实现
#include "srt_acceptor.hpp"
#include <cstring>
#include <arpa/inet.h>
#include <stdexcept>

namespace asrt {

SrtAcceptor::SrtAcceptor(SrtReactor& reactor)
    : reactor_(reactor), sock_(SRT_INVALID_SOCK) {
    
    // 创建 SRT socket
    sock_ = srt_create_socket();
    if (sock_ == SRT_INVALID_SOCK) {
        const auto err_msg = std::string("Failed to create SRT acceptor socket: ") + srt_getlasterror_str();
        ASRT_LOG_ERROR("{}", err_msg);
        throw std::runtime_error(err_msg);
    }
    
    ASRT_LOG_DEBUG("SrtAcceptor created (fd={})", sock_);
    
    // 设置为非阻塞模式（这是 post 选项，可以立即设置）
    int no = 0;
    srt_setsockopt(sock_, 0, SRTO_RCVSYN, &no, sizeof(no));
    srt_setsockopt(sock_, 0, SRTO_SNDSYN, &no, sizeof(no));
}

SrtAcceptor::SrtAcceptor(const std::map<std::string, std::string>& options, SrtReactor& reactor)
    : reactor_(reactor), sock_(SRT_INVALID_SOCK), options_(options) {
    
    // 创建 SRT socket
    sock_ = srt_create_socket();
    if (sock_ == SRT_INVALID_SOCK) {
        const auto err_msg = std::string("Failed to create SRT acceptor socket: ") + srt_getlasterror_str();
        ASRT_LOG_ERROR("{}", err_msg);
        throw std::runtime_error(err_msg);
    }
    
    ASRT_LOG_DEBUG("SrtAcceptor created with options (fd={})", sock_);
    
    // 设置为非阻塞模式
    int no = 0;
    srt_setsockopt(sock_, 0, SRTO_RCVSYN, &no, sizeof(no));
    srt_setsockopt(sock_, 0, SRTO_SNDSYN, &no, sizeof(no));
}

SrtAcceptor::SrtAcceptor(SrtAcceptor&& other) noexcept
    : reactor_(other.reactor_),
      sock_(other.sock_),
      options_(std::move(other.options_)),
      listener_callback_(std::move(other.listener_callback_)),
      options_applied_pre_bind_(other.options_applied_pre_bind_),
      options_applied_pre_(other.options_applied_pre_) {
    other.sock_ = SRT_INVALID_SOCK;
}

SrtAcceptor& SrtAcceptor::operator=(SrtAcceptor&& other) noexcept {
    if (this != &other) {
        close();
        // reactor_ is a reference, cannot be reassigned
        // Assuming both refer to the same reactor
        sock_ = other.sock_;
        options_ = std::move(other.options_);
        listener_callback_ = std::move(other.listener_callback_);
        options_applied_pre_bind_ = other.options_applied_pre_bind_;
        options_applied_pre_ = other.options_applied_pre_;
        other.sock_ = SRT_INVALID_SOCK;
    }
    return *this;
}

SrtAcceptor::~SrtAcceptor() {
    close();
}

void SrtAcceptor::close() {
    if (sock_ != SRT_INVALID_SOCK) {
        ASRT_LOG_DEBUG("Closing SrtAcceptor (fd={})", sock_);
        
        // 清理回调
        srt_listen_callback(sock_, nullptr, nullptr);
        
        srt_close(sock_);
        sock_ = SRT_INVALID_SOCK;
    }
}

bool SrtAcceptor::apply_pre_bind_options() {
    // 新版本中，PRE 选项包含了所有需要在绑定/监听前设置的选项
    // 这里为了兼容性保留这个函数，但实际上在 apply_pre_options 中处理
    return true;
}

bool SrtAcceptor::apply_pre_options() {
    if (options_applied_pre_) {
        return true;  // 已应用
    }
    
    auto failures = options_.apply_pre(sock_);
    if (!failures.empty()) {
        std::string failed_options;
        for (const auto& f : failures) {
            failed_options += f + " ";
        }
        ASRT_LOG_WARNING("Failed to apply pre options: {}", failed_options);
    }
    
    options_applied_pre_ = true;
    return failures.empty();
}

// SRT 原生监听回调（静态）
int SrtAcceptor::srt_listen_callback_fn(void* opaq, SRTSOCKET ns, int hsversion,
                                        const struct sockaddr* peeraddr, const char* streamid) {
    SrtAcceptor* acceptor = static_cast<SrtAcceptor*>(opaq);
    if (!acceptor || !acceptor->listener_callback_) {
        return 0;  // 默认接受
    }
    
    // 转换地址为字符串
    std::string peer_addr = sockaddr_to_string(peeraddr);
    std::string stream_id = streamid ? streamid : "";
    
    ASRT_LOG_DEBUG("SRT listen callback: socket={}, hsversion={}, peer={}, streamid={}", ns, hsversion, peer_addr, stream_id);
    
    // 创建临时 SrtSocket 对象用于回调
    // 注意：这个 socket 还没有被接受，只是用于回调中设置选项
    SrtSocket temp_socket(ns, acceptor->reactor_);
    
    try {
        // 调用用户回调
        int result = acceptor->listener_callback_(temp_socket, hsversion, stream_id);
        
        if (result == 0) {
            ASRT_LOG_INFO("Connection accepted from {}", peer_addr);
        } else {
            ASRT_LOG_INFO("Connection rejected from {}", peer_addr);
        }
        
        // 释放临时 socket 的所有权（不关闭实际的socket）
        // 通过友元访问私有成员
        temp_socket.sock_ = SRT_INVALID_SOCK;  // 只是释放所有权，不关闭socket
        
        return result;
    } catch (const std::exception& e) {
        ASRT_LOG_ERROR("Exception in listener callback: {}", e.what());
        // 释放临时 socket 的所有权
        temp_socket.sock_ = SRT_INVALID_SOCK;
        return -1;  // 拒绝连接
    } catch (...) {
        ASRT_LOG_ERROR("Unknown exception in listener callback");
        // 释放临时 socket 的所有权
        temp_socket.sock_ = SRT_INVALID_SOCK;
        return -1;  // 拒绝连接
    }
}

void SrtAcceptor::set_listener_callback(ListenerCallback callback) {
    listener_callback_ = std::move(callback);
    
    if (sock_ != SRT_INVALID_SOCK) {
        // 设置 SRT 原生回调
        if (listener_callback_) {
            int result = srt_listen_callback(sock_, &SrtAcceptor::srt_listen_callback_fn, this);
            if (result == SRT_ERROR) {
                ASRT_LOG_WARNING("Failed to set listen callback: {}", srt_getlasterror_str());
            } else {
                ASRT_LOG_DEBUG("Listen callback set successfully");
            }
        } else {
            // 清除回调
            srt_listen_callback(sock_, nullptr, nullptr);
            ASRT_LOG_DEBUG("Listen callback cleared");
        }
    }
}

// ========================================
// 监听操作
// ========================================

void SrtAcceptor::bind(const std::string& address, uint16_t port, int backlog) {
    if (!is_open()) {
        throw std::runtime_error("Acceptor is not open");
    }
    
    ASRT_LOG_INFO("Binding to {}:{} (fd={})", address, port, sock_);
    
    // 应用 pre-bind 选项
    if (!apply_pre_bind_options()) {
        ASRT_LOG_WARNING("Some pre-bind options failed to apply");
    }
    
    // 设置本地地址
    sockaddr_in sa;
    std::memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    
    if (address == "0.0.0.0" || address.empty()) {
        sa.sin_addr.s_addr = INADDR_ANY;
    } else {
        if (inet_pton(AF_INET, address.c_str(), &sa.sin_addr) != 1) {
            const auto err_msg = std::string("Invalid IP address: ") + address;
            ASRT_LOG_ERROR("{}", err_msg);
            throw std::runtime_error(err_msg);
        }
    }
    
    // 绑定
    if (srt_bind(sock_, reinterpret_cast<sockaddr*>(&sa), sizeof(sa)) == SRT_ERROR) {
        const auto err_msg = std::string("Failed to bind: ") + srt_getlasterror_str();
        ASRT_LOG_ERROR("{}", err_msg);
        throw std::runtime_error(err_msg);
    }
    
    // 应用 pre 选项
    if (!apply_pre_options()) {
        ASRT_LOG_WARNING("Some pre options failed to apply");
    }
    
    // 设置 SRT 原生监听回调（如果有）
    if (listener_callback_) {
        int result = srt_listen_callback(sock_, &SrtAcceptor::srt_listen_callback_fn, this);
        if (result == SRT_ERROR) {
            ASRT_LOG_WARNING("Failed to set listen callback: {}", srt_getlasterror_str());
        }
    }
    
    // 监听
    if (srt_listen(sock_, backlog) == SRT_ERROR) {
        const auto err_msg = std::string("Failed to listen: ") + srt_getlasterror_str();
        ASRT_LOG_ERROR("{}", err_msg);
        throw std::runtime_error(err_msg);
    }
    
    ASRT_LOG_INFO("Listening on {} (fd={}, backlog={})", local_address(), sock_, backlog);
}

// ========================================
// 接受连接
// ========================================

SRTSOCKET SrtAcceptor::try_accept(sockaddr_storage& client_addr, int& addr_len, std::error_code& ec) {
    addr_len = sizeof(client_addr);
    SRTSOCKET client_sock = srt_accept(sock_, reinterpret_cast<sockaddr*>(&client_addr), &addr_len);
    
    if (client_sock == SRT_INVALID_SOCK) {
        int srt_err = srt_getlasterror(nullptr);
        if (srt_err == SRT_EASYNCRCV) {
            // Would block，返回 INVALID 表示需要等待
            ec = std::make_error_code(std::errc::operation_would_block);
            return SRT_INVALID_SOCK;
        } else {
            // 真正的错误
            const char* error_msg = nullptr;
            ec = make_srt_error_code(error_msg);
            
            if(error_msg)
                ASRT_LOG_ERROR("Accept failed (fd={}): {} ({})", sock_, ec.message(), error_msg);
            else
                ASRT_LOG_ERROR("Accept failed (fd={}): {}", sock_, ec.message());

            return SRT_INVALID_SOCK;
        }
    }
    
    ec.clear();
    return client_sock;
}

asio::awaitable<SrtSocket> SrtAcceptor::async_accept() {
    if (!is_open()) {
        throw std::runtime_error("Acceptor is not open");
    }
    
    ASRT_LOG_DEBUG("Waiting for incoming connection...");
    
    while (true) {
        // 先尝试直接接受
        sockaddr_storage client_addr;
        int addr_len = 0;
        std::error_code ec;
        
        SRTSOCKET client_sock = try_accept(client_addr, addr_len, ec);
        
        if (client_sock != SRT_INVALID_SOCK) {
            // 成功接受连接
            std::string client_addr_str = sockaddr_to_string(reinterpret_cast<sockaddr*>(&client_addr));
            
            ASRT_LOG_INFO("Accepted connection from {} (client_fd={})", client_addr_str, client_sock);
            
            // 创建 SrtSocket 对象
            // 注意：如果设置了 listener callback，它已经在 srt_accept 之前被调用了
            SrtSocket client_socket(client_sock, reactor_);
            
            // 应用 post 选项（从 acceptor 继承）
            client_socket.options_ = options_;
            client_socket.apply_post_options();
            
            co_return client_socket;
        }
        
        if (ec && ec != std::errc::operation_would_block) {
            // 真正的错误
            throw asio::system_error(ec);
        }
        
        // Would block，等待可读
        ASRT_LOG_DEBUG("No pending connection, waiting...");
        co_await reactor_.async_wait_readable(sock_);
    }
}

asio::awaitable<SrtSocket> SrtAcceptor::async_accept(std::chrono::milliseconds timeout) {
    if (!is_open()) {
        throw std::runtime_error("Acceptor is not open");
    }
    
    ASRT_LOG_DEBUG("Waiting for incoming connection with timeout {}ms...", timeout.count());
    
    while (true) {
        // 先尝试直接接受
        sockaddr_storage client_addr;
        int addr_len = 0;
        std::error_code ec;
        
        SRTSOCKET client_sock = try_accept(client_addr, addr_len, ec);
        
        if (client_sock != SRT_INVALID_SOCK) {
            // 成功接受连接
            std::string client_addr_str = sockaddr_to_string(reinterpret_cast<sockaddr*>(&client_addr));
            
            ASRT_LOG_INFO("Accepted connection from {} (client_fd={})", client_addr_str, client_sock);
            
            // 创建 SrtSocket 对象
            SrtSocket client_socket(client_sock, reactor_);
            
            // 应用 post 选项（从 acceptor 继承）
            client_socket.options_ = options_;
            client_socket.apply_post_options();
            
            co_return client_socket;
        }
        
        if (ec && ec != std::errc::operation_would_block) {
            // 真正的错误
            throw asio::system_error(ec);
        }
        
        // Would block，等待可读（带超时）
        ASRT_LOG_DEBUG("No pending connection, waiting with timeout...");
        co_await reactor_.async_wait_readable(sock_, timeout);
    }
}

// ========================================
// 状态查询
// ========================================

std::string SrtAcceptor::local_address() const {
    if (!is_open()) {
        return "";
    }
    
    sockaddr_storage addr;
    int addr_len = sizeof(addr);
    
    if (srt_getsockname(sock_, reinterpret_cast<sockaddr*>(&addr), &addr_len) == SRT_ERROR) {
        return "";
    }
    
    return sockaddr_to_string(reinterpret_cast<sockaddr*>(&addr));
}

std::string SrtAcceptor::sockaddr_to_string(const sockaddr* addr) {
    if (!addr) {
        return "";
    }
    
    char ip_str[INET6_ADDRSTRLEN];
    uint16_t port = 0;
    
    if (addr->sa_family == AF_INET) {
        auto* addr_in = reinterpret_cast<const sockaddr_in*>(addr);
        inet_ntop(AF_INET, &addr_in->sin_addr, ip_str, sizeof(ip_str));
        port = ntohs(addr_in->sin_port);
    } else if (addr->sa_family == AF_INET6) {
        auto* addr_in6 = reinterpret_cast<const sockaddr_in6*>(addr);
        inet_ntop(AF_INET6, &addr_in6->sin6_addr, ip_str, sizeof(ip_str));
        port = ntohs(addr_in6->sin6_port);
    } else {
        return "";
    }
    
    return std::string(ip_str) + ":" + std::to_string(port);
}

} // namespace asrt
