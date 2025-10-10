// srt_log.h - 统一的日志系统，支持自定义回调
#pragma once

#include <srt/srt.h>
#include <string>
#include <iostream>
#include <functional>
#include <fmt/format.h>
#include <mutex>
#include <string_view>

namespace asrt {

// 简化的日志级别（映射到 SRT 的日志级别）
enum class LogLevel {
    Debug = LOG_DEBUG,        // 详细调试信息
    Notice = LOG_NOTICE,      // 一般通知（默认）
    Warning = LOG_WARNING,    // 警告
    Error = LOG_ERR,          // 错误
    Critical = LOG_CRIT       // 严重错误
};

// 用户自定义日志回调
// 参数：level - 日志级别, area - 区域（Reactor/SRT等）, message - 日志内容
//       file - 源文件名, function - 函数名, line - 行号
using LogCallback = std::function<void(LogLevel level, const char* area, const char* message,
                                      const char* file, const char* function, int line)>;

// 统一的日志系统
// - 自动将 Reactor 和 SRT 库的日志统一处理
// - 支持自定义日志回调
// - 线程安全
class SrtLog {
public:
    // 初始化 SRT 日志系统（在 Reactor 启动时调用）
    static void init(LogLevel level = LogLevel::Notice) {

        // 保存日志级别
        get_level_ref() = level;
        
        // 设置 SRT 日志级别
        srt_setloglevel(static_cast<int>(level));
        
        // 设置日志处理器（所有日志都会通过这里）
        srt_setloghandler(nullptr, log_handler);
        
        // 启用所有功能区域的日志
        srt_resetlogfa(nullptr, 0);
    }
    
    // 设置日志级别
    static void set_level(LogLevel level) {
        get_level_ref() = level;
        srt_setloglevel(static_cast<int>(level));
    }
    
    // 获取当前日志级别
    static LogLevel get_level() {
        return get_level_ref();
    }
    
    // 设置自定义日志回调
    // callback: nullptr 表示使用默认输出（stderr）
    static void set_callback(LogCallback callback) {
        get_callback_ref() = std::move(callback);
    }
    
    template<typename... Args>
    static void log(LogLevel level, const char* area,
                   const char* file , const char* function , int line,
                   const std::string_view fmt_str, Args&&... args) {
        // 由于SRT的log_handler不支持function参数，我们需要单独处理
        // 如果是Reactor的日志且有用户回调，直接调用用户回调
        
        // 级别过滤
        if (static_cast<int>(level) > static_cast<int>(get_level_ref())) {
            return;
        }

        auto message = fmt::format(fmt::runtime(fmt_str), std::forward<Args>(args)...);
        auto& callback = get_callback_ref();
        if (callback && area && std::string(area) == "Reactor") {
            // Reactor日志，直接调用用户回调，保留function信息
            callback(level, area, message.c_str(), file, function, line);
        } else {
            // SRT库日志或无用户回调，使用标准处理器
            log_handler(nullptr, static_cast<int>(level), file, line, area, message.c_str());
        }
    }

private:
    // SRT 日志处理器（同时处理 Reactor 和 SRT 库的所有日志）
    static void log_handler(void* /*opaque*/, int level, const char* file, int line, 
                           const char* area, const char* message) {

        // 转换日志级别
        LogLevel log_level = static_cast<LogLevel>(level);
        
        // 级别过滤
        if (level > static_cast<int>(get_level_ref())) {
            return;
        }
        
        // 调用用户回调或默认处理器
        auto& callback = get_callback_ref();
        if (callback) {
            // 用户自定义回调
            // 注意：对于SRT库的日志，function参数为空
            callback(log_level, area, message, file, "", line);
        } else {
            // 默认输出到 stderr
            const char* level_str = "";
            switch (level) {
                case LOG_DEBUG:   level_str = "DEBUG"; break;
                case LOG_NOTICE:  level_str = "INFO "; break;
                case LOG_WARNING: level_str = "WARN "; break;
                case LOG_ERR:     level_str = "ERROR"; break;
                case LOG_CRIT:    level_str = "FATAL"; break;
                default:          level_str = "?????"; break;
            }
            
            // 输出格式：[级别] [区域] [文件:函数:行号] 消息
            std::cerr << "[" << level_str << "] [" << area << "] ";
            
            // 如果有文件信息，添加调用位置
            if (file && *file) {
                std::cerr << "[" << file;
                if (line > 0) {
                    std::cerr << ":" << line;
                }
                std::cerr << "] ";
            }
            
            std::cerr << message << std::endl;
        }
    }

    static LogCallback& get_callback_ref() {
        static LogCallback callback;
        return callback;
    }
    
    static LogLevel& get_level_ref() {
        static LogLevel level = LogLevel::Notice;
        return level;
    }
};

} // namespace asrt

// 便捷的日志宏（自动添加文件名、函数名、行号）
#define ASRT_LOG_DEBUG(fmt, ...) \
    asrt::SrtLog::log(asrt::LogLevel::Debug, "Reactor", __FILE__, __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)

#define ASRT_LOG_INFO(fmt, ...) \
    asrt::SrtLog::log(asrt::LogLevel::Notice, "Reactor", __FILE__, __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)

#define ASRT_LOG_WARNING(fmt, ...) \
    asrt::SrtLog::log(asrt::LogLevel::Warning, "Reactor", __FILE__, __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)

#define ASRT_LOG_ERROR(fmt, ...) \
    asrt::SrtLog::log(asrt::LogLevel::Error, "Reactor", __FILE__, __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)
