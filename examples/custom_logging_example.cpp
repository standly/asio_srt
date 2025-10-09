// custom_logging_example.cpp - 自定义日志输出示例

#include "asrt/srt_reactor.hpp"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <chrono>
#include <memory>
using namespace asrt;
// ========================================
// 示例 1：自定义格式输出到 stdout
// ========================================

void example1_custom_format() {
    std::cout << "\n=== 示例 1：自定义日志格式 ===\n" << std::endl;
    
    // 设置自定义日志回调
    SrtReactor::set_log_callback([](asrt::LogLevel level, const char* area, const char* message) {
        // 自定义时间格式
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        
        // 自定义级别标签
        const char* level_emoji = "";
        switch (level) {
            case asrt::LogLevel::Debug:    level_emoji = "🐛"; break;
            case asrt::LogLevel::Notice:   level_emoji = "ℹ️ "; break;
            case asrt::LogLevel::Warning:  level_emoji = "⚠️ "; break;
            case asrt::LogLevel::Error:    level_emoji = "❌"; break;
            case asrt::LogLevel::Critical: level_emoji = "💀"; break;
        }
        
        // 输出自定义格式
        std::cout << level_emoji << " "
                  << std::put_time(std::localtime(&time_t), "%H:%M:%S")
                  << "." << std::setfill('0') << std::setw(3) << ms.count()
                  << " [" << area << "] " << message << std::endl;
    });
    
    // 使用 Reactor（会输出自定义格式的日志）
    auto& reactor = SrtReactor::get_instance();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

// ========================================
// 示例 2：输出到文件
// ========================================

void example2_file_logging() {
    std::cout << "\n=== 示例 2：输出到文件 ===\n" << std::endl;
    
    // 创建日志文件
    auto log_file = std::make_shared<std::ofstream>("/tmp/srt_reactor.log", std::ios::app);
    
    if (!log_file->is_open()) {
        std::cerr << "无法打开日志文件" << std::endl;
        return;
    }
    
    // 设置文件输出回调
    SrtReactor::set_log_callback([log_file](asrt::LogLevel level, const char* area, const char* message) {
        // 添加时间戳
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        
        // 转换级别
        const char* level_str = "";
        switch (level) {
            case asrt::LogLevel::Debug:    level_str = "DEBUG"; break;
            case asrt::LogLevel::Notice:   level_str = "INFO "; break;
            case asrt::LogLevel::Warning:  level_str = "WARN "; break;
            case asrt::LogLevel::Error:    level_str = "ERROR"; break;
            case asrt::LogLevel::Critical: level_str = "FATAL"; break;
        }
        
        // 写入文件
        *log_file << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S")
                  << " [" << level_str << "] [" << area << "] " 
                  << message << std::endl;
        log_file->flush(); // 立即刷新
    });
    
    std::cout << "日志正在写入 /tmp/srt_reactor.log" << std::endl;
    
    // 使用 Reactor
    auto& reactor = SrtReactor::get_instance();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    std::cout << "日志已写入文件" << std::endl;
}

// ========================================
// 示例 3：集成到 spdlog
// ========================================

void example3_spdlog_integration() {
    std::cout << "\n=== 示例 3：集成到 spdlog (伪代码) ===\n" << std::endl;
    
    // 伪代码：假设你已经有一个 spdlog logger
    // auto logger = spdlog::stdout_color_mt("reactor");
    
    SrtReactor::set_log_callback([](asrt::LogLevel level, const char* area, const char* message) {
        // 映射到 spdlog 级别
        /*
        switch (level) {
            case asrt::LogLevel::Debug:
                logger->debug("[{}] {}", area, message);
                break;
            case asrt::LogLevel::Notice:
                logger->info("[{}] {}", area, message);
                break;
            case asrt::LogLevel::Warning:
                logger->warn("[{}] {}", area, message);
                break;
            case asrt::LogLevel::Error:
                logger->error("[{}] {}", area, message);
                break;
            case asrt::LogLevel::Critical:
                logger->critical("[{}] {}", area, message);
                break;
        }
        */
        std::cout << "（这里会调用你的 spdlog logger）" << std::endl;
    });
    
    std::cout << "可以轻松集成到任何日志库（spdlog, glog, log4cpp 等）" << std::endl;
}

// ========================================
// 示例 4：按区域过滤日志
// ========================================

void example4_area_filtering() {
    std::cout << "\n=== 示例 4：只记录 Reactor 的日志，忽略 SRT 库的日志 ===\n" << std::endl;
    
    SrtReactor::set_log_callback([](asrt::LogLevel level, const char* area, const char* message) {
        // 只输出 Reactor 的日志
        std::string area_str(area);
        if (area_str == "Reactor") {
            std::cout << "[Reactor] " << message << std::endl;
        }
        // SRT 库的日志被忽略
    });
    
    SrtReactor::set_log_level(asrt::LogLevel::Debug);
    auto& reactor = SrtReactor::get_instance();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

// ========================================
// 示例 5：结构化日志（JSON 格式）
// ========================================

void example5_structured_logging() {
    std::cout << "\n=== 示例 5：结构化日志（JSON 格式）===\n" << std::endl;
    
    SrtReactor::set_log_callback([](asrt::LogLevel level, const char* area, const char* message) {
        // 转换级别为字符串
        const char* level_str = "";
        switch (level) {
            case asrt::LogLevel::Debug:    level_str = "debug"; break;
            case asrt::LogLevel::Notice:   level_str = "info"; break;
            case asrt::LogLevel::Warning:  level_str = "warning"; break;
            case asrt::LogLevel::Error:    level_str = "error"; break;
            case asrt::LogLevel::Critical: level_str = "critical"; break;
        }
        
        // 获取时间戳
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
        
        // 输出 JSON 格式
        std::cout << "{"
                  << "\"timestamp\":" << timestamp << ","
                  << "\"level\":\"" << level_str << "\","
                  << "\"area\":\"" << area << "\","
                  << "\"message\":\"" << message << "\""
                  << "}" << std::endl;
    });
    
    auto& reactor = SrtReactor::get_instance();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

// ========================================
// 示例 6：恢复默认日志输出
// ========================================

void example6_restore_default() {
    std::cout << "\n=== 示例 6：恢复默认日志输出 ===\n" << std::endl;
    
    // 恢复默认的 stderr 输出
    SrtReactor::set_log_callback(nullptr);
    
    std::cout << "现在使用默认格式：" << std::endl;
    auto& reactor = SrtReactor::get_instance();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

// ========================================
// 主函数
// ========================================

int main() {
    std::cout << "=== asio_srt 自定义日志示例 ===" << std::endl;
    
    // 运行各个示例
    example1_custom_format();
    example2_file_logging();
    example3_spdlog_integration();
    example4_area_filtering();
    example5_structured_logging();
    example6_restore_default();
    
    std::cout << "\n=== 所有示例完成 ===" << std::endl;
    
    return 0;
}




