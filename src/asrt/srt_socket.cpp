// srt_socket_v2.cpp - 更新的 SRT socket 实现
#include "srt_socket.hpp"
#include <sstream>
#include <cstring>
#include <arpa/inet.h>

namespace asrt {

// ========================================
// SrtSocket 实现
// ========================================

SrtSocket::SrtSocket(SrtReactor& reactor)
    : reactor_(reactor), sock_(SRT_INVALID_SOCK) {
    
    // 创建 SRT socket
    sock_ = srt_create_socket();
    if (sock_ == SRT_INVALID_SOCK) {
        std::ostringstream oss;
        oss << "Failed to create SRT socket: " << srt_getlasterror_str();
        ASRT_LOG_ERROR(oss.str());
        throw std::runtime_error(oss.str());
    }
    
    std::ostringstream oss;
    oss << "SrtSocket created (fd=" << sock_ << ")";
    ASRT_LOG_DEBUG(oss.str());
    
    // 默认设置为非阻塞模式（这是 post 选项，可以立即设置）
    int no = 0;
    srt_setsockopt(sock_, 0, SRTO_RCVSYN, &no, sizeof(no));
    srt_setsockopt(sock_, 0, SRTO_SNDSYN, &no, sizeof(no));
}

SrtSocket::SrtSocket(const std::map<std::string, std::string>& options, SrtReactor& reactor)
    : reactor_(reactor), sock_(SRT_INVALID_SOCK), options_(options) {
    
    // 创建 SRT socket
    sock_ = srt_create_socket();
    if (sock_ == SRT_INVALID_SOCK) {
        std::ostringstream oss;
        oss << "Failed to create SRT socket: " << srt_getlasterror_str();
        ASRT_LOG_ERROR(oss.str());
        throw std::runtime_error(oss.str());
    }
    
    std::ostringstream oss;
    oss << "SrtSocket created with options (fd=" << sock_ << ")";
    ASRT_LOG_DEBUG(oss.str());
    
    // 默认设置为非阻塞模式
    int no = 0;
    srt_setsockopt(sock_, 0, SRTO_RCVSYN, &no, sizeof(no));
    srt_setsockopt(sock_, 0, SRTO_SNDSYN, &no, sizeof(no));
    
    // 立即应用 post 选项
    apply_post_options();
}

SrtSocket::SrtSocket(SRTSOCKET sock, SrtReactor& reactor)
    : reactor_(reactor), sock_(sock) {
    
    std::ostringstream oss;
    oss << "SrtSocket created from accepted socket (fd=" << sock_ << ")";
    ASRT_LOG_DEBUG(oss.str());
    
    // 确保非阻塞
    int no = 0;
    srt_setsockopt(sock_, 0, SRTO_RCVSYN, &no, sizeof(no));
    srt_setsockopt(sock_, 0, SRTO_SNDSYN, &no, sizeof(no));
    
    // 对于已接受的 socket，标记选项已应用
    options_applied_pre_bind_ = true;
    options_applied_pre_ = true;
}

SrtSocket::SrtSocket(SrtSocket&& other) noexcept
    : reactor_(other.reactor_),
      sock_(other.sock_),
      options_(std::move(other.options_)),
      connect_callback_(std::move(other.connect_callback_)),
      options_applied_pre_bind_(other.options_applied_pre_bind_),
      options_applied_pre_(other.options_applied_pre_) {
    other.sock_ = SRT_INVALID_SOCK;
}

SrtSocket& SrtSocket::operator=(SrtSocket&& other) noexcept {
    if (this != &other) {
        close();
        // reactor_ is a reference, cannot be reassigned
        // Assuming both refer to the same reactor
        sock_ = other.sock_;
        options_ = std::move(other.options_);
        connect_callback_ = std::move(other.connect_callback_);
        options_applied_pre_bind_ = other.options_applied_pre_bind_;
        options_applied_pre_ = other.options_applied_pre_;
        other.sock_ = SRT_INVALID_SOCK;
    }
    return *this;
}

SrtSocket::~SrtSocket() {
    close();
}

void SrtSocket::close() {
    if (sock_ != SRT_INVALID_SOCK) {
        std::ostringstream oss;
        oss << "Closing SrtSocket (fd=" << sock_ << ")";
        ASRT_LOG_DEBUG(oss.str());
        
        srt_close(sock_);
        sock_ = SRT_INVALID_SOCK;
    }
}

bool SrtSocket::apply_pre_bind_options() {
    // 新版本中，PRE 选项包含了所有需要在连接前设置的选项
    // 这里为了兼容性保留这个函数，但实际上在 apply_pre_options 中处理
    return true;
}

bool SrtSocket::apply_pre_options() {
    if (options_applied_pre_) {
        return true;  // 已应用
    }
    
    auto failures = options_.apply_pre(sock_);
    if (!failures.empty()) {
        std::ostringstream oss;
        oss << "Failed to apply pre options: ";
        for (const auto& f : failures) {
            oss << f << " ";
        }
        ASRT_LOG_WARNING(oss.str());
    }
    
    options_applied_pre_ = true;
    return failures.empty();
}

bool SrtSocket::apply_post_options() {
    auto failures = options_.apply_post(sock_);
    if (!failures.empty()) {
        std::ostringstream oss;
        oss << "Failed to apply post options: ";
        for (const auto& f : failures) {
            oss << f << " ";
        }
        ASRT_LOG_WARNING(oss.str());
    }
    return failures.empty();
}

// SRT 原生连接回调（静态）
void SrtSocket::srt_connect_callback_fn(void* opaq, SRTSOCKET ns, int errorcode,
                                        const struct sockaddr* peeraddr, int token) {
    SrtSocket* socket = static_cast<SrtSocket*>(opaq);
    if (!socket || !socket->connect_callback_) {
        return;
    }
    
    std::ostringstream oss;
    oss << "SRT connect callback: socket=" << ns << ", error=" << errorcode 
        << ", token=" << token;
    ASRT_LOG_DEBUG(oss.str());
    
    // 转换错误码
    std::error_code ec;
    if (errorcode != 0) {
        ec = make_srt_error_code();
    }
    
    // 在 reactor 的 io_context 中调用用户回调
    auto& io_context = socket->reactor_.get_io_context();
    asio::post(io_context, [socket, ec]() {
        try {
            socket->connect_callback_(ec, *socket);
        } catch (const std::exception& e) {
            ASRT_LOG_ERROR("Exception in connect callback: " + std::string(e.what()));
        } catch (...) {
            ASRT_LOG_ERROR("Unknown exception in connect callback");
        }
    });
}

// ========================================
// 连接操作
// ========================================

asio::awaitable<void> SrtSocket::async_connect(const std::string& host, uint16_t port) {
    if (!is_open()) {
        throw std::runtime_error("Socket is not open");
    }
    
    std::ostringstream oss;
    oss << "Connecting to " << host << ":" << port << " (fd=" << sock_ << ")";
    ASRT_LOG_INFO(oss.str());
    
    // 应用 pre 选项（pre-bind 选项对于客户端通常不需要，除非绑定本地地址）
    if (!apply_pre_options()) {
        ASRT_LOG_WARNING("Some pre options failed to apply");
    }
    
    // 设置 SRT 原生连接回调
    if (connect_callback_) {
        int result = srt_connect_callback(sock_, &SrtSocket::srt_connect_callback_fn, this);
        if (result == SRT_ERROR) {
            ASRT_LOG_WARNING("Failed to set connect callback: " + std::string(srt_getlasterror_str()));
        }
    }
    
    // 设置目标地址
    sockaddr_in sa;
    std::memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    
    if (inet_pton(AF_INET, host.c_str(), &sa.sin_addr) != 1) {
        std::ostringstream err_oss;
        err_oss << "Invalid IP address: " << host;
        ASRT_LOG_ERROR(err_oss.str());
        
        std::error_code ec = std::make_error_code(std::errc::invalid_argument);
        if (connect_callback_) {
            connect_callback_(ec, *this);
        }
        throw asio::system_error(ec);
    }
    
    // 尝试连接（非阻塞）
    int result = srt_connect(sock_, reinterpret_cast<sockaddr*>(&sa), sizeof(sa));
    
    if (result == SRT_ERROR) {
        int srt_err = srt_getlasterror(nullptr);
        
        // 如果是异步连接中，等待可写事件（表示连接完成）
        if (srt_err == SRT_EASYNCRCV || srt_err == SRT_EASYNCSND) {
            ASRT_LOG_DEBUG("Connection in progress, waiting...");
            
            try {
                // 等待 socket 可写（连接完成）
                co_await reactor_.async_wait_writable(sock_);
                
                // 检查连接状态
                SRT_SOCKSTATUS st = srt_getsockstate(sock_);
                if (st != SRTS_CONNECTED) {
                    const char* error_msg = nullptr;
                    std::error_code ec = make_srt_error_code(error_msg);
                    
                    std::ostringstream err_oss;
                    err_oss << "Connection failed: " << ec.message();
                    if (error_msg) {
                        err_oss << " (" << error_msg << ")";
                    }
                    ASRT_LOG_ERROR(err_oss.str());
                    
                    // 注意：原生回调可能已经被调用
                    throw asio::system_error(ec);
                }
                
                std::ostringstream success_oss;
                success_oss << "Connected successfully (fd=" << sock_ << ")";
                ASRT_LOG_INFO(success_oss.str());
                
                // 应用 post 选项
                apply_post_options();
                
                // 注意：原生回调会自动调用
                co_return;
            } catch (...) {
                // 如果等待失败，清理回调
                srt_connect_callback(sock_, nullptr, nullptr);
                throw;
            }
        } else {
            // 其他错误
            const char* error_msg = nullptr;
            std::error_code ec = make_srt_error_code(error_msg);
            
            std::ostringstream err_oss;
            err_oss << "Connection failed immediately: " << ec.message();
            if (error_msg) {
                err_oss << " (" << error_msg << ")";
            }
            ASRT_LOG_ERROR(err_oss.str());
            
            // 清理回调
            srt_connect_callback(sock_, nullptr, nullptr);
            
            if (connect_callback_) {
                connect_callback_(ec, *this);
            }
            throw asio::system_error(ec);
        }
    }
    
    // 连接立即成功（不太可能，但处理这种情况）
    std::ostringstream success_oss;
    success_oss << "Connected immediately (fd=" << sock_ << ")";
    ASRT_LOG_INFO(success_oss.str());
    
    // 应用 post 选项
    apply_post_options();
    
    // 注意：原生回调会自动调用
    co_return;
}

asio::awaitable<void> SrtSocket::async_connect(const std::string& host, uint16_t port,
                                                 std::chrono::milliseconds timeout) {
    if (!is_open()) {
        throw std::runtime_error("Socket is not open");
    }
    
    std::ostringstream oss;
    oss << "Connecting to " << host << ":" << port << " with timeout " 
        << timeout.count() << "ms (fd=" << sock_ << ")";
    ASRT_LOG_INFO(oss.str());
    
    // 应用 pre 选项
    if (!apply_pre_options()) {
        ASRT_LOG_WARNING("Some pre options failed to apply");
    }
    
    // 设置 SRT 原生连接回调
    if (connect_callback_) {
        int result = srt_connect_callback(sock_, &SrtSocket::srt_connect_callback_fn, this);
        if (result == SRT_ERROR) {
            ASRT_LOG_WARNING("Failed to set connect callback: " + std::string(srt_getlasterror_str()));
        }
    }
    
    // 设置目标地址
    sockaddr_in sa;
    std::memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    
    if (inet_pton(AF_INET, host.c_str(), &sa.sin_addr) != 1) {
        std::ostringstream err_oss;
        err_oss << "Invalid IP address: " << host;
        ASRT_LOG_ERROR(err_oss.str());
        
        std::error_code ec = std::make_error_code(std::errc::invalid_argument);
        
        // 清理回调
        srt_connect_callback(sock_, nullptr, nullptr);
        
        if (connect_callback_) {
            connect_callback_(ec, *this);
        }
        throw asio::system_error(ec);
    }
    
    // 尝试连接（非阻塞）
    int result = srt_connect(sock_, reinterpret_cast<sockaddr*>(&sa), sizeof(sa));
    
    if (result == SRT_ERROR) {
        int srt_err = srt_getlasterror(nullptr);
        
        if (srt_err == SRT_EASYNCRCV || srt_err == SRT_EASYNCSND) {
            ASRT_LOG_DEBUG("Connection in progress, waiting with timeout...");
            
            try {
                // 等待 socket 可写（带超时）
                co_await reactor_.async_wait_writable(sock_, timeout);
                
                // 检查连接状态
                SRT_SOCKSTATUS st = srt_getsockstate(sock_);
                if (st != SRTS_CONNECTED) {
                    const char* error_msg = nullptr;
                    std::error_code ec = make_srt_error_code(error_msg);
                    
                    std::ostringstream err_oss;
                    err_oss << "Connection failed: " << ec.message();
                    if (error_msg) {
                        err_oss << " (" << error_msg << ")";
                    }
                    ASRT_LOG_ERROR(err_oss.str());
                    
                    // 清理回调
                    srt_connect_callback(sock_, nullptr, nullptr);
                    
                    throw asio::system_error(ec);
                }
                
                std::ostringstream success_oss;
                success_oss << "Connected successfully (fd=" << sock_ << ")";
                ASRT_LOG_INFO(success_oss.str());
                
                // 应用 post 选项
                apply_post_options();
                
                co_return;
            } catch (...) {
                // 清理回调
                srt_connect_callback(sock_, nullptr, nullptr);
                throw;
            }
        } else {
            const char* error_msg = nullptr;
            std::error_code ec = make_srt_error_code(error_msg);
            
            std::ostringstream err_oss;
            err_oss << "Connection failed immediately: " << ec.message();
            if (error_msg) {
                err_oss << " (" << error_msg << ")";
            }
            ASRT_LOG_ERROR(err_oss.str());
            
            // 清理回调
            srt_connect_callback(sock_, nullptr, nullptr);
            
            if (connect_callback_) {
                connect_callback_(ec, *this);
            }
            throw asio::system_error(ec);
        }
    }
    
    std::ostringstream success_oss;
    success_oss << "Connected immediately (fd=" << sock_ << ")";
    ASRT_LOG_INFO(success_oss.str());
    
    // 应用 post 选项
    apply_post_options();
    
    co_return;
}

// ========================================
// 读写操作（基于 Packet）
// ========================================

int SrtSocket::try_recv_packet(char* data, size_t max_size, std::error_code& ec) {
    int bytes = srt_recvmsg(sock_, data, max_size);
    
    if (bytes == SRT_ERROR) {
        int srt_err = srt_getlasterror(nullptr);
        if (srt_err == SRT_EASYNCRCV) {
            // Would block，返回 0 表示需要等待
            ec = std::make_error_code(std::errc::operation_would_block);
            return 0;
        } else {
            // 真正的错误
            const char* error_msg = nullptr;
            ec = make_srt_error_code(error_msg);
            
            std::ostringstream oss;
            oss << "Receive failed (fd=" << sock_ << "): " << ec.message();
            if (error_msg) {
                oss << " (" << error_msg << ")";
            }
            ASRT_LOG_ERROR(oss.str());
            return -1;
        }
    }
    
    ec.clear();
    return bytes;
}

int SrtSocket::try_send_packet(const char* data, size_t size, std::error_code& ec) {
    int bytes = srt_sendmsg(sock_, data, size, -1, true);
    
    if (bytes == SRT_ERROR) {
        int srt_err = srt_getlasterror(nullptr);
        if (srt_err == SRT_EASYNCSND) {
            // Would block，返回 0 表示需要等待
            ec = std::make_error_code(std::errc::operation_would_block);
            return 0;
        } else {
            // 真正的错误
            const char* error_msg = nullptr;
            ec = make_srt_error_code(error_msg);
            
            std::ostringstream oss;
            oss << "Send failed (fd=" << sock_ << "): " << ec.message();
            if (error_msg) {
                oss << " (" << error_msg << ")";
            }
            ASRT_LOG_ERROR(oss.str());
            return -1;
        }
    }
    
    ec.clear();
    return bytes;
}

asio::awaitable<size_t> SrtSocket::async_read_packet(char* data, size_t max_size) {
    if (!is_open()) {
        throw std::runtime_error("Socket is not open");
    }
    
    std::ostringstream oss;
    oss << "Reading packet (fd=" << sock_ << ", max_size=" << max_size << ")";
    ASRT_LOG_DEBUG(oss.str());
    
    while (true) {
        // 先尝试直接读取
        std::error_code ec;
        int bytes = try_recv_packet(data, max_size, ec);
        
        if (bytes > 0) {
            // 成功读取
            std::ostringstream success_oss;
            success_oss << "Read " << bytes << " bytes (fd=" << sock_ << ")";
            ASRT_LOG_DEBUG(success_oss.str());
            co_return bytes;
        }
        
        if (ec && ec != std::errc::operation_would_block) {
            // 真正的错误
            throw asio::system_error(ec);
        }
        
        // Would block，等待可读
        ASRT_LOG_DEBUG("Socket would block, waiting for readable...");
        co_await reactor_.async_wait_readable(sock_);
    }
}

asio::awaitable<size_t> SrtSocket::async_read_packet(char* data, size_t max_size,
                                                       std::chrono::milliseconds timeout) {
    if (!is_open()) {
        throw std::runtime_error("Socket is not open");
    }
    
    std::ostringstream oss;
    oss << "Reading packet with timeout " << timeout.count() 
        << "ms (fd=" << sock_ << ", max_size=" << max_size << ")";
    ASRT_LOG_DEBUG(oss.str());
    
    while (true) {
        // 先尝试直接读取
        std::error_code ec;
        int bytes = try_recv_packet(data, max_size, ec);
        
        if (bytes > 0) {
            std::ostringstream success_oss;
            success_oss << "Read " << bytes << " bytes (fd=" << sock_ << ")";
            ASRT_LOG_DEBUG(success_oss.str());
            co_return bytes;
        }
        
        if (ec && ec != std::errc::operation_would_block) {
            throw asio::system_error(ec);
        }
        
        // Would block，等待可读（带超时）
        ASRT_LOG_DEBUG("Socket would block, waiting for readable with timeout...");
        co_await reactor_.async_wait_readable(sock_, timeout);
    }
}

asio::awaitable<size_t> SrtSocket::async_write_packet(const char* data, size_t size) {
    if (!is_open()) {
        throw std::runtime_error("Socket is not open");
    }
    
    std::ostringstream oss;
    oss << "Writing packet (fd=" << sock_ << ", size=" << size << ")";
    ASRT_LOG_DEBUG(oss.str());
    
    while (true) {
        // 先尝试直接写入
        std::error_code ec;
        int bytes = try_send_packet(data, size, ec);
        
        if (bytes > 0) {
            // 成功写入
            std::ostringstream success_oss;
            success_oss << "Wrote " << bytes << " bytes (fd=" << sock_ << ")";
            ASRT_LOG_DEBUG(success_oss.str());
            co_return bytes;
        }
        
        if (ec && ec != std::errc::operation_would_block) {
            // 真正的错误
            throw asio::system_error(ec);
        }
        
        // Would block，等待可写
        ASRT_LOG_DEBUG("Socket would block, waiting for writable...");
        co_await reactor_.async_wait_writable(sock_);
    }
}

asio::awaitable<size_t> SrtSocket::async_write_packet(const char* data, size_t size,
                                                        std::chrono::milliseconds timeout) {
    if (!is_open()) {
        throw std::runtime_error("Socket is not open");
    }
    
    std::ostringstream oss;
    oss << "Writing packet with timeout " << timeout.count() 
        << "ms (fd=" << sock_ << ", size=" << size << ")";
    ASRT_LOG_DEBUG(oss.str());
    
    while (true) {
        // 先尝试直接写入
        std::error_code ec;
        int bytes = try_send_packet(data, size, ec);
        
        if (bytes > 0) {
            std::ostringstream success_oss;
            success_oss << "Wrote " << bytes << " bytes (fd=" << sock_ << ")";
            ASRT_LOG_DEBUG(success_oss.str());
            co_return bytes;
        }
        
        if (ec && ec != std::errc::operation_would_block) {
            throw asio::system_error(ec);
        }
        
        // Would block，等待可写（带超时）
        ASRT_LOG_DEBUG("Socket would block, waiting for writable with timeout...");
        co_await reactor_.async_wait_writable(sock_, timeout);
    }
}

// ========================================
// 状态查询
// ========================================

SRT_SOCKSTATUS SrtSocket::status() const {
    if (!is_open()) {
        return SRTS_CLOSED;
    }
    return srt_getsockstate(sock_);
}

bool SrtSocket::get_stats(SRT_TRACEBSTATS& stats) const {
    if (!is_open()) {
        return false;
    }
    
    int result = srt_bstats(sock_, &stats, true);
    if (result == SRT_ERROR) {
        std::ostringstream oss;
        oss << "Failed to get stats (fd=" << sock_ << "): " << srt_getlasterror_str();
        ASRT_LOG_ERROR(oss.str());
        return false;
    }
    
    return true;
}

std::string SrtSocket::local_address() const {
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

std::string SrtSocket::remote_address() const {
    if (!is_open()) {
        return "";
    }
    
    sockaddr_storage addr;
    int addr_len = sizeof(addr);
    
    if (srt_getpeername(sock_, reinterpret_cast<sockaddr*>(&addr), &addr_len) == SRT_ERROR) {
        return "";
    }
    
    return sockaddr_to_string(reinterpret_cast<sockaddr*>(&addr));
}

std::string SrtSocket::sockaddr_to_string(const sockaddr* addr) {
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
    
    std::ostringstream oss;
    oss << ip_str << ":" << port;
    return oss.str();
}

} // namespace asrt
