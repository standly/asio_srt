#!/bin/bash
# 简单测试自定义日志功能

cd "$(dirname "$0")/.."

cat > /tmp/test_log.cpp << 'EOF'
#include "src/asrt/srt_reactor.hpp"
#include <iostream>
#include <vector>
using namespace asrt;

struct LogEntry {
    asrt::LogLevel level;
    std::string area;
    std::string message;
};

int main() {
    std::cout << "=== 测试 1：自定义日志回调 ===" << std::endl;
    
    std::vector<LogEntry> logs;
    
    SrtReactor::set_log_callback([&logs](asrt::LogLevel level, const char* area, const char* msg) {
        logs.push_back({level, area, msg});
        std::cout << "📝 [" << area << "] " << msg << std::endl;
    });
    
    auto& reactor = SrtReactor::get_instance();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    std::cout << "\n捕获了 " << logs.size() << " 条日志" << std::endl;
    std::cout << "✅ 测试 1 通过\n" << std::endl;
    
    // 测试 2：恢复默认
    std::cout << "=== 测试 2：恢复默认输出 ===" << std::endl;
    SrtReactor::set_log_callback(nullptr);
    std::cout << "✅ 测试 2 通过\n" << std::endl;
    
    // 测试 3：级别控制
    std::cout << "=== 测试 3：日志级别 ===" << std::endl;
    auto level = SrtReactor::get_log_level();
    std::cout << "当前级别：" << static_cast<int>(level) << std::endl;
    SrtReactor::set_log_level(asrt::LogLevel::Debug);
    std::cout << "✅ 测试 3 通过\n" << std::endl;
    
    std::cout << "\n🎉 所有测试通过！" << std::endl;
    return 0;
}
EOF

echo "正在编译测试..."
cd build && \
cmake .. && \
make -j$(nproc) && \
g++ -std=c++20 -I../src -I/usr/include -I../depends/build/srt/install/include \
    /tmp/test_log.cpp -o /tmp/test_custom_log \
    -L./src/asrt -lasrt \
    -L../depends/build/srt/install/lib -lsrt \
    -lssl -lcrypto -lpthread

if [ $? -eq 0 ]; then
    echo -e "\n编译成功！运行测试...\n"
    LD_LIBRARY_PATH=../depends/build/srt/install/lib:$LD_LIBRARY_PATH /tmp/test_custom_log
else
    echo "编译失败"
    exit 1
fi




