// srt_error.h
#pragma once

#include <system_error>
#include <string>
#include <srt/srt.h>

namespace asrt {

// SRT 错误码枚举
enum class srt_errc {
    success = 0,
    
    // 连接相关错误
    connection_setup = 1000,      // 连接建立失败
    connection_rejected = 1001,   // 连接被拒绝
    connection_lost = 1002,       // 连接丢失
    
    // 资源相关错误
    resource_fail = 2000,         // 资源分配失败
    thread_fail = 2001,           // 线程创建失败
    
    // 操作相关错误
    invalid_socket = 3000,        // 无效的 socket
    epoll_add_failed = 3001,      // 添加到 epoll 失败
    epoll_update_failed = 3002,   // 更新 epoll 失败
    
    // 数据传输错误
    send_failed = 4000,           // 发送失败
    recv_failed = 4001,           // 接收失败
    
    // 超时
    timeout = 5000,               // 操作超时
};

// SRT 错误类别
class srt_category_impl : public std::error_category {
public:
    const char* name() const noexcept override {
        return "srt";
    }
    
    std::string message(int ev) const override {
        switch (static_cast<srt_errc>(ev)) {
            case srt_errc::success:
                return "Success";
                
            case srt_errc::connection_setup:
                return "Connection setup failed";
            case srt_errc::connection_rejected:
                return "Connection rejected";
            case srt_errc::connection_lost:
                return "Connection lost";
                
            case srt_errc::resource_fail:
                return "Resource allocation failed";
            case srt_errc::thread_fail:
                return "Thread creation failed";
                
            case srt_errc::invalid_socket:
                return "Invalid socket";
            case srt_errc::epoll_add_failed:
                return "Failed to add socket to epoll";
            case srt_errc::epoll_update_failed:
                return "Failed to update epoll";
                
            case srt_errc::send_failed:
                return "Send operation failed";
            case srt_errc::recv_failed:
                return "Receive operation failed";
                
            case srt_errc::timeout:
                return "Operation timed out";
                
            default:
                return "Unknown SRT error";
        }
    }
    
    // 映射到标准错误条件，使错误码可以与 ASIO 兼容
    std::error_condition default_error_condition(int ev) const noexcept override {
        switch (static_cast<srt_errc>(ev)) {
            case srt_errc::success:
                return std::error_condition();
                
            case srt_errc::connection_setup:
            case srt_errc::connection_rejected:
                return std::errc::connection_refused;
                
            case srt_errc::connection_lost:
                return std::errc::connection_reset;
                
            case srt_errc::resource_fail:
                return std::errc::not_enough_memory;
                
            case srt_errc::thread_fail:
                return std::errc::resource_unavailable_try_again;
                
            case srt_errc::invalid_socket:
                return std::errc::bad_file_descriptor;
                
            case srt_errc::epoll_add_failed:
            case srt_errc::epoll_update_failed:
                return std::errc::io_error;
                
            case srt_errc::send_failed:
            case srt_errc::recv_failed:
                return std::errc::io_error;
                
            case srt_errc::timeout:
                return std::errc::timed_out;
                
            default:
                return std::error_condition(ev, *this);
        }
    }
};

// 获取 SRT 错误类别的单例
inline const std::error_category& srt_category() noexcept {
    static srt_category_impl instance;
    return instance;
}

// 从 srt_errc 创建 error_code
inline std::error_code make_error_code(srt_errc e) noexcept {
    return std::error_code(static_cast<int>(e), srt_category());
}

// SRT 原生错误码转换
inline std::error_code make_srt_error_code() noexcept {
    int srt_error = srt_getlasterror(nullptr);
    
    // 映射 SRT 原生错误到我们的错误码
    switch (srt_error) {
        case SRT_EINVSOCK:
            return make_error_code(srt_errc::invalid_socket);
            
        case SRT_ECONNSETUP:
            return make_error_code(srt_errc::connection_setup);
            
        case SRT_ECONNREJ:
            return make_error_code(srt_errc::connection_rejected);
            
        case SRT_ECONNLOST:
            return make_error_code(srt_errc::connection_lost);
            
        case SRT_ERESOURCE:
            return make_error_code(srt_errc::resource_fail);
            
        case SRT_ETHREAD:
            return make_error_code(srt_errc::thread_fail);
            
        case SRT_EASYNCSND:
        case SRT_EASYNCRCV:
            return std::make_error_code(std::errc::operation_would_block);
            
        case SRT_ETIMEOUT:
            return make_error_code(srt_errc::timeout);
            
        default:
            // 未知错误，使用通用 I/O 错误
            return std::make_error_code(std::errc::io_error);
    }
}

// 带详细信息的错误码创建
inline std::error_code make_srt_error_code(const char* &error_msg) noexcept {
    error_msg = srt_getlasterror_str();
    return make_srt_error_code();
}

} // namespace asrt

// 注册为 std::error_code
namespace std {
    template <>
    struct is_error_code_enum<asrt::srt_errc> : true_type {};
}

