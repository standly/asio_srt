// test_logging_enhancement.cpp - 测试增强的日志功能
#include <gtest/gtest.h>
#include "asrt/srt_reactor.hpp"
#include "asrt/srt_socket.hpp"
#include "asrt/srt_acceptor.hpp"
#include <vector>
#include <string>
#include <regex>
#include <thread>
#include <chrono>
#include <mutex>

using namespace asrt;
using namespace std::chrono_literals;

// 日志条目结构
struct LogEntry {
    LogLevel level;
    std::string area;
    std::string message;
    std::string file;
    std::string function;
    int line;
};

class LoggingEnhancementTest : public ::testing::Test {
protected:
    std::vector<LogEntry> captured_logs;
    std::mutex captured_logs_mutex;
    
    void SetUp() override {
        // 清空日志
        {
            std::lock_guard<std::mutex> lock(captured_logs_mutex);
            captured_logs.clear();
        }
        
        // 设置自定义日志回调
        SrtReactor::set_log_callback(
            [this](LogLevel level, const char* area, const char* message,
                   const char* file, const char* function, int line) {
                std::lock_guard<std::mutex> lock(captured_logs_mutex);
                captured_logs.push_back({
                    level, 
                    area ? area : "", 
                    message ? message : "",
                    file ? file : "",
                    function ? function : "",
                    line
                });
            }
        );
        
        // 设置日志级别为DEBUG以捕获所有日志
        SrtReactor::set_log_level(LogLevel::Debug);
    }
    
    void TearDown() override {
        // 恢复默认日志处理
        SrtReactor::set_log_callback(nullptr);
        SrtReactor::set_log_level(LogLevel::Notice);
    }
    
    // 帮助函数：检查日志条目是否包含文件信息
    bool hasFileInfo(const LogEntry& entry) {
        return !entry.file.empty() && !entry.function.empty() && entry.line > 0;
    }
    
    // 帮助函数：查找包含特定消息的日志
    std::vector<LogEntry> findLogsWithMessage(const std::string& msg) {
        std::lock_guard<std::mutex> lock(captured_logs_mutex);
        std::vector<LogEntry> result;
        for (const auto& log : captured_logs) {
            if (log.message.find(msg) != std::string::npos) {
                result.push_back(log);
            }
        }
        return result;
    }
};

// Test 1: 验证日志包含文件名、函数名和行号
TEST_F(LoggingEnhancementTest, LogContainsFileInfo) {
    // 触发一些日志
    auto& reactor = SrtReactor::get_instance();
    
    // 创建一个socket来触发日志
    SrtSocket socket;
    
    // 等待一下让日志产生
    std::this_thread::sleep_for(100ms);
    
    // 验证至少有一些日志被捕获
    {
        std::lock_guard<std::mutex> lock(captured_logs_mutex);
        ASSERT_GT(captured_logs.size(), 0) << "应该捕获到一些日志";
    }
    
    // 检查是否有包含文件信息的日志
    bool found_with_file_info = false;
    {
        std::lock_guard<std::mutex> lock(captured_logs_mutex);
        for (const auto& log : captured_logs) {
            if (hasFileInfo(log)) {
                found_with_file_info = true;
                
                // 验证文件名格式
                EXPECT_TRUE(log.file.find(".cpp") != std::string::npos || 
                           log.file.find(".hpp") != std::string::npos)
                    << "文件名应该是.cpp或.hpp文件: " << log.file;
                
                // 验证函数名不为空
                EXPECT_FALSE(log.function.empty()) 
                    << "函数名不应该为空";
                
                // 验证行号合理
                EXPECT_GT(log.line, 0) 
                    << "行号应该大于0";
                
                std::cout << "日志示例 - 文件: " << log.file 
                         << ", 函数: " << log.function 
                         << ", 行: " << log.line 
                         << ", 消息: " << log.message << std::endl;
            }
        }
    }
    
    EXPECT_TRUE(found_with_file_info) << "应该找到包含文件信息的日志";
}

// Test 2: 验证不同日志级别的文件信息
TEST_F(LoggingEnhancementTest, DifferentLogLevelsWithFileInfo) {
    // 创建acceptor来触发不同级别的日志
    SrtAcceptor acceptor;
    
    try {
        // 尝试绑定到一个已使用的端口（可能失败）
        acceptor.bind("127.0.0.1", 0);
    } catch (...) {
        // 忽略错误
    }
    
    std::this_thread::sleep_for(100ms);
    
    // 检查不同级别的日志
    std::map<LogLevel, int> level_counts;
    {
        std::lock_guard<std::mutex> lock(captured_logs_mutex);
        for (const auto& log : captured_logs) {
            if (hasFileInfo(log)) {
                level_counts[log.level]++;
            }
        }
    }
    
    // 至少应该有一些日志
    int total_with_file_info = 0;
    for (const auto& [level, count] : level_counts) {
        total_with_file_info += count;
        std::cout << "级别 " << static_cast<int>(level) 
                 << " 的日志数: " << count << std::endl;
    }
    
    EXPECT_GT(total_with_file_info, 0) << "应该有带文件信息的日志";
}

// Test 3: 验证自定义区域的日志
TEST_F(LoggingEnhancementTest, CustomAreaLogs) {
    // 直接使用日志宏
    ASRT_LOG_DEBUG("测试DEBUG日志");
    ASRT_LOG_INFO("测试INFO日志");
    ASRT_LOG_WARNING("测试WARNING日志");
    ASRT_LOG_ERROR("测试ERROR日志");
    
    std::this_thread::sleep_for(50ms);
    
    // 查找Reactor区域的日志
    int reactor_logs = 0;
    {
        std::lock_guard<std::mutex> lock(captured_logs_mutex);
        for (const auto& log : captured_logs) {
            if (log.area == "Reactor" && hasFileInfo(log)) {
                reactor_logs++;
                
                // 验证这些日志来自测试文件
                EXPECT_TRUE(log.file.find("test_logging_enhancement.cpp") != std::string::npos)
                    << "日志应该来自测试文件，实际: " << log.file;
                
                // 验证函数名（在Google Test中，函数名会是TestBody）
                EXPECT_TRUE(log.function == "TestBody" || log.function.find("CustomAreaLogs") != std::string::npos)
                    << "函数名应该是TestBody或包含测试函数名，实际: " << log.function;
            }
        }
    }
    
    EXPECT_GE(reactor_logs, 4) << "应该至少有4条Reactor区域的日志";
}

// Test 4: 验证日志格式化输出
TEST_F(LoggingEnhancementTest, LogFormatting) {
    // 临时切换到默认日志处理器以查看格式
    SrtReactor::set_log_callback(nullptr);
    
    // 捕获stderr输出
    testing::internal::CaptureStderr();
    
    // 产生一条日志
    ASRT_LOG_INFO("格式化测试消息");
    
    std::string output = testing::internal::GetCapturedStderr();
    
    // 验证输出格式包含文件信息
    // 格式应该类似: [INFO ] [Reactor] [file:function:line] message
    EXPECT_TRUE(output.find("[INFO ]") != std::string::npos) 
        << "应该包含日志级别";
    EXPECT_TRUE(output.find("[Reactor]") != std::string::npos) 
        << "应该包含区域";
    EXPECT_TRUE(output.find("test_logging_enhancement.cpp") != std::string::npos) 
        << "应该包含文件名";
    EXPECT_TRUE(output.find(":") != std::string::npos) 
        << "应该包含分隔符";
    
    std::cout << "默认格式输出: " << output;
}

// Test 5: 验证文件路径处理
TEST_F(LoggingEnhancementTest, FilePathHandling) {
    // 产生一些日志
    ASRT_LOG_DEBUG("路径测试");
    
    std::this_thread::sleep_for(50ms);
    
    auto logs = findLogsWithMessage("路径测试");
    ASSERT_GT(logs.size(), 0) << "应该找到测试日志";
    
    for (const auto& log : logs) {
        if (hasFileInfo(log)) {
            // 文件路径可能是绝对路径或相对路径
            // 但应该以.cpp或.hpp结尾
            std::string file = log.file;
            EXPECT_TRUE(file.length() > 4) << "文件路径应该有合理长度";
            
            // 提取文件名（去掉路径）
            size_t last_slash = file.find_last_of("/\\");
            std::string filename = (last_slash != std::string::npos) 
                ? file.substr(last_slash + 1) : file;
            
            std::cout << "完整路径: " << file << std::endl;
            std::cout << "文件名: " << filename << std::endl;
            
            // 验证是C++源文件
            EXPECT_TRUE(filename.find(".cpp") != std::string::npos || 
                       filename.find(".hpp") != std::string::npos)
                << "应该是C++源文件";
        }
    }
}

// Test 6: 验证并发日志记录
TEST_F(LoggingEnhancementTest, ConcurrentLogging) {
    std::atomic<int> thread_count{0};
    const int num_threads = 4;
    const int logs_per_thread = 10;
    
    std::vector<std::thread> threads;
    
    // 启动多个线程同时记录日志
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            thread_count++;
            
            // 等待所有线程就绪
            while (thread_count < num_threads) {
                std::this_thread::yield();
            }
            
            // 同时记录日志
            for (int j = 0; j < logs_per_thread; ++j) {
                std::string msg = "线程" + std::to_string(i) + 
                                 " 日志" + std::to_string(j);
                ASRT_LOG_INFO(msg);
            }
        });
    }
    
    // 等待所有线程完成
    for (auto& t : threads) {
        t.join();
    }
    
    std::this_thread::sleep_for(100ms);
    
    // 验证所有日志都被记录
    int concurrent_logs = 0;
    {
        std::lock_guard<std::mutex> lock(captured_logs_mutex);
        for (const auto& log : captured_logs) {
            if (log.message.find("线程") != std::string::npos && 
                hasFileInfo(log)) {
                concurrent_logs++;
            }
        }
        
        EXPECT_EQ(concurrent_logs, num_threads * logs_per_thread)
            << "应该记录所有并发日志";
        
        // 验证所有日志都有完整的文件信息
        for (const auto& log : captured_logs) {
            if (log.message.find("线程") != std::string::npos) {
                EXPECT_TRUE(hasFileInfo(log))
                    << "并发日志应该包含文件信息";
            }
        }
    }
}

// Test 7: 验证SRT库日志的文件信息
TEST_F(LoggingEnhancementTest, SrtLibraryLogs) {
    // 创建socket触发SRT库的日志
    SRTSOCKET sock = srt_create_socket();
    ASSERT_NE(sock, SRT_INVALID_SOCK);
    
    // 设置一些选项来触发日志
    int yes = 1;
    srt_setsockopt(sock, 0, SRTO_RCVSYN, &yes, sizeof(yes));
    
    // 尝试连接到无效地址以触发错误日志
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(12345);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    
    // 这会失败但会产生日志
    srt_connect(sock, (sockaddr*)&addr, sizeof(addr));
    
    srt_close(sock);
    
    std::this_thread::sleep_for(100ms);
    
    // 查找SRT库的日志
    int srt_logs = 0;
    {
        std::lock_guard<std::mutex> lock(captured_logs_mutex);
        for (const auto& log : captured_logs) {
            if (log.area != "Reactor") {  // 非Reactor区域的日志可能来自SRT库
                srt_logs++;
                std::cout << "SRT库日志 - 区域: " << log.area 
                         << ", 消息: " << log.message;
                if (hasFileInfo(log)) {
                    std::cout << ", 文件: " << log.file 
                             << ":" << log.line;
                }
                std::cout << std::endl;
            }
        }
    }
    
    // SRT库的日志可能没有文件信息（取决于SRT的构建方式）
    std::cout << "捕获的SRT库日志数: " << srt_logs << std::endl;
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
