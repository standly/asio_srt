# asio_srt

一个将 **SRT (Secure Reliable Transport)** 协议与 **ASIO** 异步 I/O 库集成的 C++ 项目。

## 📋 项目概述

**asio_srt** 旨在为 SRT 协议提供现代 C++ 异步编程接口，利用 C++20 协程特性实现高效的异步网络通信。该项目将 SRT 的低延迟、高可靠性传输能力与 ASIO 的异步 I/O 模型相结合。

> ⚠️ **注意**: 本项目目前处于开发阶段，尚未完成所有功能。

## 🎯 核心目标

- 将 SRT 的低延迟、高可靠性传输能力与 ASIO 的异步 I/O 模型结合
- 提供基于 C++20 协程的现代异步 API
- 实现 Reactor 模式来处理 SRT socket 事件

## 🏗️ 技术架构

### 技术栈

- **C++ 标准**: C++20（使用协程特性）
- **网络库**: ASIO 1.36.0（header-only）
- **传输协议**: SRT 1.5.4（Secure Reliable Transport）
- **构建系统**: CMake 3.20+

### 错误处理

`asio_srt` 使用标准的 `std::error_code` 体系，与 ASIO 完全兼容：

- ✅ 使用 `asio::system_error` 异常
- ✅ 支持标准错误码（`std::errc`、`asio::error`）
- ✅ SRT 原生错误自动映射到标准错误
- ✅ 与 TCP/UDP 相同的错误处理模式
- ✅ **精确的错误事件检测** - 使用 `srt_epoll_uwait` 立即检测连接错误
- ✅ **详细的错误信息** - 包含 SRT 原生错误消息

**示例**：
```cpp
try {
    // 等待可读（带超时）
    co_await reactor.async_wait_readable(sock, 5000ms);
    
    // 如果成功返回，socket 真的可读
    char buffer[1500];
    int n = srt_recv(sock, buffer, sizeof(buffer));
    
} catch (const asio::system_error& e) {
    if (e.code() == std::make_error_code(std::errc::timed_out)) {
        // 超时处理
    } else if (e.code() == std::make_error_code(std::errc::connection_reset)) {
        // 连接断开
    } else {
        // 其他错误
        std::cerr << "Error: " << e.what() << "\n";
    }
}
```

详见：
- [错误处理指南](docs/ERROR_HANDLING.md)
- [错误事件实现](docs/ERROR_EVENT_IMPLEMENTATION.md)

### 日志系统

`asio_srt` 统一使用 **SRT 库的日志系统**，支持自定义输出：

- ✅ **统一的日志输出** - Reactor 和 SRT 协议栈使用同一日志系统
- ✅ **自定义回调** - 支持输出到任何目标（文件、spdlog、数据库等）
- ✅ **可配置的日志级别** - Debug / Notice / Warning / Error / Critical
- ✅ **简洁的输出格式** - `[级别] [区域] 消息`
- ✅ **零配置** - 默认输出到 stderr，开箱即用
- ✅ **线程安全** - 所有 API 都是线程安全的

**快速开始（默认输出）**：
```cpp
#include "asrt/srt_reactor.h"

int main() {
    // 可选：设置日志级别（默认：Notice）
    SrtReactor::set_log_level(asrt::LogLevel::Debug);
    
    // 使用 Reactor（会自动输出日志到 stderr）
    auto& reactor = SrtReactor::get_instance();
    
    // 你的代码...
    
    return 0;
}
```

**自定义日志输出**：
```cpp
// 示例 1：输出到文件
auto log_file = std::make_shared<std::ofstream>("reactor.log");
SrtReactor::set_log_callback([log_file](asrt::LogLevel level, const char* area, const char* msg) {
    *log_file << "[" << area << "] " << msg << std::endl;
});

// 示例 2：集成到 spdlog
auto logger = spdlog::stdout_color_mt("reactor");
SrtReactor::set_log_callback([logger](asrt::LogLevel level, const char* area, const char* msg) {
    logger->info("[{}] {}", area, msg);
});

// 示例 3：恢复默认输出
SrtReactor::set_log_callback(nullptr);
```

**日志输出示例**：
```
[INFO ] [Reactor] SrtReactor started
[DEBUG] [Reactor] Socket 123 added to epoll (events=0x9)
[DEBUG] [SRT] Sending packet seq=1234  ← SRT 协议栈的日志
[ERROR] [Reactor] Socket 123 error: Connection reset [events=0xd]
```

**日志级别**：
- `Debug` - 详细调试信息（socket 添加、事件触发、SRT 协议细节）
- `Notice` - 一般通知（启动、停止、连接建立）- **默认**
- `Warning` - 警告信息
- `Error` - 错误信息（连接断开、IO 错误）
- `Critical` - 严重错误

**完整文档**：[日志系统使用指南](docs/LOGGING_ENHANCED.md)

### 核心组件

#### 1. SrtReactor（反应器）

单例模式设计的核心组件，提供以下特性：

- **双线程架构**：
  - ASIO 线程：运行 `io_context`，处理异步操作
  - SRT 轮询线程：使用 `srt_epoll_wait` 监控 SRT socket 事件
- 支持同时等待读/写事件
- 集成取消操作支持
- 使用 strand 保证线程安全

#### 2. 异步 API

```cpp
// 等待 socket 可读
asio::awaitable<int> async_wait_readable(SRTSOCKET srt_sock);

// 等待 socket 可写
asio::awaitable<int> async_wait_writable(SRTSOCKET srt_sock);

// 带超时的版本
asio::awaitable<int> async_wait_readable(SRTSOCKET srt_sock, std::chrono::milliseconds timeout);
asio::awaitable<int> async_wait_writable(SRTSOCKET srt_sock, std::chrono::milliseconds timeout);
```

**特性**：
- 返回 `asio::awaitable`，支持 `co_await`
- 自动处理错误（失败时抛出异常）
- 支持操作取消
- **超时版本**: 超时返回 -1，成功返回事件标志

#### 3. 事件管理

- 使用 `EventOperation` 结构分别管理读/写处理器
- 动态更新 SRT epoll 事件掩码
- 自动清理完成的操作

## 📂 项目结构

```
asio_srt/
├── 3rdparty/
│   └── asio-1.36.0/          # ASIO 头文件库
├── depends/
│   ├── pkgs/                 # 依赖包（srt-1.5.4.tar.gz 等）
│   ├── resolved/             # 已编译的依赖库
│   ├── scripts/              # 依赖管理脚本（depends.cmake）
│   └── build/                # 依赖构建目录
├── src/
│   ├── asrt/                 # 核心 SRT+ASIO 集成模块
│   │   ├── srt_reactor.h     # Reactor 接口定义
│   │   └── srt_reactor.cpp   # Reactor 实现
│   ├── aentry/               # 应用入口（待完成）
│   └── core/                 # 核心功能模块（待开发）
└── tests/                    # 测试代码（待开发）
```

## ⚙️ 核心实现细节

### 反应器模式实现

- 使用 SRT 的 epoll 机制监控 socket 事件
- 通过 ASIO 的 strand 确保操作的线程安全性
- 支持多个 socket 的并发操作

### 异步操作生命周期

1. **注册**：将 handler 添加到 `EventOperation`，更新 SRT epoll
2. **触发**：poll 线程检测到事件后，通过 strand 分发到 ASIO 线程
3. **完成/取消**：清理 handler，更新或移除 epoll 监控

### 错误处理

- SRT 操作失败时返回 `std::errc::io_error`
- 取消操作时返回 `asio::error::operation_aborted`
- 协程自动传播异常

## 🚧 当前开发状态

### 已完成 ✅

- SrtReactor 核心框架完整实现
- 读/写异步等待 API
- 事件循环和轮询机制
- 取消操作支持
- CMake 构建配置和依赖管理
- 完整的单元测试套件（Google Test）

### 待开发 ⏳

- `core/` 模块（空目录）
- `aentry/asrt_entry.cpp` 应用入口实现（仅占位）
- 高层封装（如 SRT socket 的 RAII 包装、连接管理等）
- 实际应用示例
- 集成测试

## 💡 适用场景

- 低延迟视频/音频流传输
- 需要可靠传输的实时数据通信
- 需要异步 I/O 的 SRT 应用开发
- 跨公网的媒体流传输

## 🔧 构建说明

### 前置要求

- CMake 3.20 或更高版本
- C++20 兼容的编译器（GCC 10+, Clang 12+）
- SRT 库（已包含在 `depends/pkgs/` 中）

### 构建步骤

```bash
# 1. 克隆仓库
git clone <repository-url>
cd asio_srt

# 2. 创建构建目录
mkdir build && cd build

# 3. 配置和构建
cmake ..
make

# 4. 运行测试
ctest --output-on-failure
# 或直接运行测试可执行文件
./tests/test_srt_reactor
```

### 依赖管理

项目使用自定义的依赖管理系统（`depends/scripts/depends.cmake`），SRT 库需要预先编译并放置在 `depends/resolved/srt/` 目录下。

## 🧪 测试

项目使用 **Google Test** 框架进行单元测试。

### 测试覆盖

- ✅ 单例模式验证
- ✅ IO 上下文可用性
- ✅ Socket 读写操作
- ✅ 数据发送接收
- ✅ 并发操作支持
- ✅ 操作取消机制
- ✅ 同时读写操作
- ✅ 错误处理

### 运行测试

```bash
# 运行所有测试
cd build
ctest --output-on-failure

# 运行特定测试
./tests/test_srt_reactor --gtest_filter=SrtReactorTest.SendReceiveData

# 查看详细测试列表
./tests/test_srt_reactor --gtest_list_tests
```

详细测试文档请参见 [tests/README.md](tests/README.md)。

## 📖 使用示例

### 基本使用

```cpp
#include "asrt/srt_reactor.h"

asio::awaitable<void> basic_example() {
    auto& reactor = SrtReactor::get_instance();
    
    // 创建 SRT socket
    SRTSOCKET sock = srt_create_socket();
    
    // 等待 socket 可写
    int events = co_await reactor.async_wait_writable(sock);
    
    // 发送数据
    const char* data = "Hello, SRT!";
    srt_send(sock, data, strlen(data));
    
    // 等待 socket 可读
    events = co_await reactor.async_wait_readable(sock);
    
    // 接收数据
    char buffer[1500];
    int received = srt_recv(sock, buffer, sizeof(buffer));
}
```

### 带超时的使用

```cpp
#include "asrt/srt_reactor.h"
#include <chrono>

using namespace std::chrono_literals;

asio::awaitable<void> timeout_example() {
    auto& reactor = SrtReactor::get_instance();
    SRTSOCKET sock = srt_create_socket();
    
    // 等待最多 5 秒接收数据
    int result = co_await reactor.async_wait_readable(sock, 5000ms);
    
    if (result == -1) {
        std::cout << "Timeout: No data received\n";
    } else {
        // 接收数据
        char buffer[1500];
        int received = srt_recv(sock, buffer, sizeof(buffer));
    }
}
```

更多示例请参见 [docs/TIMEOUT_API.md](docs/TIMEOUT_API.md)。

## 🤝 贡献

欢迎提交 Issue 和 Pull Request！

## 📄 许可证

待定

## 🔗 相关链接

### 项目文档

- [日志系统使用指南](docs/LOGGING_ENHANCED.md) - 简洁易用的日志配置，支持自定义输出
- [错误处理指南](docs/ERROR_HANDLING.md) - 标准化的错误处理
- [超时 API 使用](docs/TIMEOUT_API.md) - 带超时的异步操作
- [自定义日志示例](examples/custom_logging_example.cpp) - 各种自定义日志的使用示例

### 外部资源

- [ASIO 文档](https://think-async.com/Asio/)
- [SRT 协议](https://github.com/Haivision/srt)
- [C++20 协程](https://en.cppreference.com/w/cpp/language/coroutines)

---

**当前版本**: 1.0.0 (开发中)

