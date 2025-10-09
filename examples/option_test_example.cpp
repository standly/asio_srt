// option_test_example.cpp - 测试新的选项管理系统
#include <asrt/srt_socket.hpp>
#include <asrt/srt_acceptor.hpp>
#include <asrt/srt_reactor.hpp>
#include <asrt/srt_log.hpp>
#include <iostream>
#include <map>
#include <iomanip>

using namespace asrt;

void print_option_registry() {
    std::cout << "\n=== SRT Option Registry ===" << std::endl;
    
    // PRE 选项
    std::cout << "\nPRE Options (must be set before bind/connect):" << std::endl;
    std::cout << std::setw(25) << std::left << "Option Name" 
              << std::setw(15) << "Type" 
              << "Description" << std::endl;
    std::cout << std::string(60, '-') << std::endl;
    
    const auto& pre_opts = SrtSocketOptions::get_pre_options();
    for (const auto& opt : pre_opts) {
        std::string type_str;
        switch (opt.type) {
            case SocketOption::STRING: type_str = "STRING"; break;
            case SocketOption::INT: type_str = "INT"; break;
            case SocketOption::INT64: type_str = "INT64"; break;
            case SocketOption::BOOL: type_str = "BOOL"; break;
            case SocketOption::ENUM: type_str = "ENUM"; break;
        }
        std::cout << std::setw(25) << std::left << opt.name 
                  << std::setw(15) << type_str 
                  << std::endl;
    }
    
    // POST 选项
    std::cout << "\nPOST Options (can be set anytime):" << std::endl;
    std::cout << std::setw(25) << std::left << "Option Name" 
              << std::setw(15) << "Type" 
              << "Description" << std::endl;
    std::cout << std::string(60, '-') << std::endl;
    
    const auto& post_opts = SrtSocketOptions::get_post_options();
    for (const auto& opt : post_opts) {
        std::string type_str;
        switch (opt.type) {
            case SocketOption::STRING: type_str = "STRING"; break;
            case SocketOption::INT: type_str = "INT"; break;
            case SocketOption::INT64: type_str = "INT64"; break;
            case SocketOption::BOOL: type_str = "BOOL"; break;
            case SocketOption::ENUM: type_str = "ENUM"; break;
        }
        std::cout << std::setw(25) << std::left << opt.name 
                  << std::setw(15) << type_str 
                  << std::endl;
    }
    
    std::cout << "\nTotal options: " << (pre_opts.size() + post_opts.size()) << std::endl;
}

void test_option_parsing() {
    std::cout << "\n=== Testing Option Parsing ===" << std::endl;
    
    SrtSocketOptions options;
    
    // 测试各种选项格式
    std::vector<std::string> test_options = {
        "latency=200",
        "messageapi=true",
        "sndbuf=8388608",
        "transtype=live",
        "passphrase=my_secret_key",
        "maxbw=-1",
        "oheadbw=25",
        "linger=180",
        "conntimeo=3000",
        "invalid_option=value"  // 测试未知选项
    };
    
    for (const auto& opt : test_options) {
        bool result = options.set_option(opt);
        std::cout << "Setting '" << opt << "': " 
                  << (result ? "SUCCESS" : "FAILED") << std::endl;
    }
    
    // 测试批量设置
    std::cout << "\n--- Testing batch setting ---" << std::endl;
    std::map<std::string, std::string> batch_options = {
        {"rcvbuf", "12582912"},
        {"fc", "25600"},
        {"tlpktdrop", "false"},
        {"streamid", "test/stream/123"}
    };
    
    bool batch_result = options.set_options(batch_options);
    std::cout << "Batch setting: " << (batch_result ? "SUCCESS" : "PARTIAL FAILURE") << std::endl;
    
    // 显示所有设置的选项
    std::cout << "\n--- All configured options ---" << std::endl;
    const auto& all_opts = options.get_options();
    for (const auto& [key, value] : all_opts) {
        std::cout << "  " << key << " = " << value << std::endl;
    }
}

asio::awaitable<void> test_option_application() {
    std::cout << "\n=== Testing Option Application ===" << std::endl;
    
    auto& reactor = SrtReactor::get_instance();
    
    // 测试 socket 选项
    std::cout << "\n--- Testing Socket Options ---" << std::endl;
    
    std::map<std::string, std::string> socket_options = {
        // PRE 选项
        {"messageapi", "1"},
        {"latency", "100"},
        {"conntimeo", "5000"},
        {"streamid", "test/client"},
        {"sndbuf", "4194304"},
        
        // POST 选项
        {"maxbw", "0"},
        {"inputbw", "10000000"},
        {"oheadbw", "30"},
        {"rcvsyn", "0"},
        {"sndsyn", "0"}
    };
    
    try {
        SrtSocket socket(socket_options, reactor);
        std::cout << "Socket created with options: SUCCESS" << std::endl;
        
        // 测试运行时设置选项
        bool rt_result = socket.set_option("snddropdelay=100");
        std::cout << "Runtime option setting: " << (rt_result ? "SUCCESS" : "FAILED") << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Socket creation failed: " << e.what() << std::endl;
    }
    
    // 测试 acceptor 选项
    std::cout << "\n--- Testing Acceptor Options ---" << std::endl;
    
    std::map<std::string, std::string> acceptor_options = {
        {"messageapi", "1"},
        {"latency", "200"},
        {"rcvbuf", "8388608"},
        {"payloadsize", "1316"},
        {"nakreport", "true"},
        {"fc", "32000"}
    };
    
    try {
        SrtAcceptor acceptor(acceptor_options, reactor);
        std::cout << "Acceptor created with options: SUCCESS" << std::endl;
        
        // 测试选项继承
        acceptor.bind(9999);
        std::cout << "Acceptor bound successfully" << std::endl;
        
        // 立即关闭，只是测试
        acceptor.close();
        
    } catch (const std::exception& e) {
        std::cerr << "Acceptor test failed: " << e.what() << std::endl;
    }
    
    co_return;
}

int main() {
    try {
        std::cout << "╔════════════════════════════════════════╗" << std::endl;
        std::cout << "║    SRT Option Management Test          ║" << std::endl;
        std::cout << "╚════════════════════════════════════════╝" << std::endl;
        
        // 设置日志
        SrtReactor::set_log_level(LogLevel::Debug);
        SrtReactor::set_log_callback([](LogLevel level, const char* area, const char* message,
                                       const char* file, const char* function, int line) {
            const char* level_str = "";
            switch (level) {
                case LogLevel::Debug:    level_str = "[DEBUG]"; break;
                case LogLevel::Notice:   level_str = "[INFO ]"; break;
                case LogLevel::Warning:  level_str = "[WARN ]"; break;
                case LogLevel::Error:    level_str = "[ERROR]"; break;
                case LogLevel::Critical: level_str = "[FATAL]"; break;
            }
            std::cerr << level_str << " " << message << std::endl;
        });
        
        // 1. 显示选项注册表
        print_option_registry();
        
        // 2. 测试选项解析
        test_option_parsing();
        
        // 3. 测试选项应用
        auto& reactor = SrtReactor::get_instance();
        asio::co_spawn(
            reactor.get_io_context(),
            test_option_application(),
            [](std::exception_ptr e) {
                if (e) {
                    try {
                        std::rethrow_exception(e);
                    } catch (const std::exception& ex) {
                        std::cerr << "Test failed: " << ex.what() << std::endl;
                    }
                }
            }
        );
        
        // 运行一小段时间完成测试
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        std::cout << "\n=== Test Complete ===" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
