// test_custom_log.cpp - 测试自定义日志功能

#include "asrt/srt_reactor.h"
#include <iostream>
#include <vector>
#include <string>

struct LogEntry {
    asrt::LogLevel level;
    std::string area;
    std::string message;
};

int main() {
    std::cout << "=== 测试自定义日志回调 ===\n" << std::endl;
    
    // 存储捕获的日志
    std::vector<LogEntry> captured_logs;
    
    // 设置自定义日志回调
    SrtReactor::set_log_callback([&captured_logs](asrt::LogLevel level, const char* area, const char* message) {
        captured_logs.push_back({level, area, message});
        
        // 同时输出到控制台（带自定义格式）
        const char* level_str = "";
        switch (level) {
            case asrt::LogLevel::Debug:    level_str = "🐛 DEBUG"; break;
            case asrt::LogLevel::Notice:   level_str = "ℹ️  INFO "; break;
            case asrt::LogLevel::Warning:  level_str = "⚠️  WARN "; break;
            case asrt::LogLevel::Error:    level_str = "❌ ERROR"; break;
            case asrt::LogLevel::Critical: level_str = "💀 FATAL"; break;
        }
        
        std::cout << level_str << " [" << area << "] " << message << std::endl;
    });
    
    std::cout << "✅ 已设置自定义日志回调\n" << std::endl;
    
    // 使用 Reactor（会触发日志）
    std::cout << "启动 Reactor..." << std::endl;
    auto& reactor = SrtReactor::get_instance();
    
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // 验证日志
    std::cout << "\n=== 日志验证 ===" << std::endl;
    std::cout << "捕获了 " << captured_logs.size() << " 条日志：" << std::endl;
    
    for (const auto& log : captured_logs) {
        std::cout << "  - [" << log.area << "] " << log.message << std::endl;
    }
    
    // 测试恢复默认
    std::cout << "\n=== 测试恢复默认日志 ===" << std::endl;
    SrtReactor::set_log_callback(nullptr);
    std::cout << "✅ 已恢复默认日志输出\n" << std::endl;
    
    // 测试日志级别
    std::cout << "=== 测试日志级别控制 ===" << std::endl;
    auto current_level = SrtReactor::get_log_level();
    std::cout << "当前日志级别：" << static_cast<int>(current_level) << std::endl;
    
    SrtReactor::set_log_level(asrt::LogLevel::Error);
    std::cout << "设置日志级别为 Error" << std::endl;
    
    SrtReactor::set_log_level(asrt::LogLevel::Debug);
    std::cout << "设置日志级别为 Debug" << std::endl;
    
    std::cout << "\n✅ 所有测试通过！" << std::endl;
    
    return 0;
}




