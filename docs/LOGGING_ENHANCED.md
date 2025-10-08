# 日志系统使用指南（增强版）

## 🎯 设计理念

`asio_srt` 的日志系统基于 **SRT 库的原生日志功能**，并增强了自定义能力：

- ✅ **统一输出** - Reactor 和 SRT 协议栈使用同一个日志系统
- ✅ **自定义回调** - 支持将日志输出到任何地方（文件、数据库、日志服务等）
- ✅ **简单易用** - API 简洁，只需几行代码
- ✅ **线程安全** - 所有接口都是线程安全的
- ✅ **零开销** - 不使用自定义回调时，性能与原生 SRT 日志相同

---

## 📊 日志级别

```cpp
enum class LogLevel {
    Debug = LOG_DEBUG,        // 详细调试信息
    Notice = LOG_NOTICE,      // 一般通知（默认）
    Warning = LOG_WARNING,    // 警告
    Error = LOG_ERR,          // 错误
    Critical = LOG_CRIT       // 严重错误
};
```

| 级别 | 说明 | 输出内容 |
|------|------|---------|
| **Debug** | 详细调试 | socket 添加/更新/移除、可读/可写事件、SRT 协议细节 |
| **Notice** | 一般通知（默认） | Reactor 启动/停止、SRT 连接建立 |
| **Warning** | 警告 | 潜在问题、性能警告 |
| **Error** | 错误 | socket 错误、epoll 失败、连接断开 |
| **Critical** | 严重错误 | 致命错误 |

---

## 🚀 快速开始

### 1. 默认使用（零配置）

```cpp
#include "asrt/srt_reactor.h"

int main() {
    // 直接使用，默认输出到 stderr
    auto& reactor = SrtReactor::get_instance();
    
    // 你的代码...
    
    return 0;
}
```

**输出**：
```
[INFO ] [Reactor] SrtReactor started
[DEBUG] [Reactor] Socket 123 added to epoll (events=0x9)
[DEBUG] [SRT] Sending packet seq=1234
```

### 2. 设置日志级别

```cpp
// 设置为 Debug 级别（查看所有细节）
SrtReactor::set_log_level(asrt::LogLevel::Debug);

// 获取当前级别
auto level = SrtReactor::get_log_level();
```

### 3. 自定义日志输出

```cpp
// 设置自定义回调
SrtReactor::set_log_callback([](asrt::LogLevel level, const char* area, const char* message) {
    // 你的日志处理逻辑
    std::cout << "[" << area << "] " << message << std::endl;
});

// 恢复默认输出
SrtReactor::set_log_callback(nullptr);
```

---

## 📝 API 参考

### set_log_level()

设置日志级别（同时影响 Reactor 和 SRT 协议栈）

```cpp
static void SrtReactor::set_log_level(asrt::LogLevel level);
```

**示例**：
```cpp
// 开发环境：查看所有细节
SrtReactor::set_log_level(asrt::LogLevel::Debug);

// 生产环境：只记录重要信息
SrtReactor::set_log_level(asrt::LogLevel::Notice);

// 只记录错误
SrtReactor::set_log_level(asrt::LogLevel::Error);
```

### get_log_level()

获取当前日志级别

```cpp
static asrt::LogLevel SrtReactor::get_log_level();
```

**示例**：
```cpp
auto level = SrtReactor::get_log_level();
if (level == asrt::LogLevel::Debug) {
    std::cout << "调试模式已启用" << std::endl;
}
```

### set_log_callback()

设置自定义日志回调

```cpp
static void SrtReactor::set_log_callback(asrt::LogCallback callback);
```

**参数**：
- `callback` - 日志回调函数，签名为 `void(LogLevel level, const char* area, const char* message)`
- 传入 `nullptr` 表示恢复默认输出（stderr）

**注意**：
- 回调会接收 **Reactor 和 SRT 库的所有日志**
- 回调在日志产生的线程中被调用，需要确保线程安全
- 如果回调抛出异常，行为未定义

---

## 💡 使用示例

### 示例 1：自定义格式输出

```cpp
SrtReactor::set_log_callback([](asrt::LogLevel level, const char* area, const char* message) {
    // 添加时间戳
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    // 自定义格式
    std::cout << std::put_time(std::localtime(&time_t), "%H:%M:%S")
              << " [" << area << "] " << message << std::endl;
});
```

**输出**：
```
14:32:15 [Reactor] SrtReactor started
14:32:15 [Reactor] Socket 123 added to epoll (events=0x9)
14:32:15 [SRT] Sending packet seq=1234
```

### 示例 2：输出到文件

```cpp
auto log_file = std::make_shared<std::ofstream>("reactor.log", std::ios::app);

SrtReactor::set_log_callback([log_file](asrt::LogLevel level, const char* area, const char* message) {
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
    *log_file << "[" << level_str << "] [" << area << "] " << message << std::endl;
    log_file->flush();
});
```

### 示例 3：集成 spdlog

```cpp
#include <spdlog/spdlog.h>

auto logger = spdlog::stdout_color_mt("reactor");

SrtReactor::set_log_callback([logger](asrt::LogLevel level, const char* area, const char* message) {
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
});
```

### 示例 4：按区域过滤

```cpp
SrtReactor::set_log_callback([](asrt::LogLevel level, const char* area, const char* message) {
    // 只记录 Reactor 的日志，忽略 SRT 库的日志
    std::string area_str(area);
    if (area_str == "Reactor") {
        std::cout << "[Reactor] " << message << std::endl;
    }
});
```

### 示例 5：结构化日志（JSON）

```cpp
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
    
    // 输出 JSON 格式
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    std::cout << "{"
              << "\"timestamp\":" << timestamp << ","
              << "\"level\":\"" << level_str << "\","
              << "\"area\":\"" << area << "\","
              << "\"message\":\"" << message << "\""
              << "}" << std::endl;
});
```

**输出**：
```json
{"timestamp":1696240335123,"level":"info","area":"Reactor","message":"SrtReactor started"}
{"timestamp":1696240335234,"level":"debug","area":"Reactor","message":"Socket 123 added to epoll (events=0x9)"}
{"timestamp":1696240335345,"level":"debug","area":"SRT","message":"Sending packet seq=1234"}
```

### 示例 6：多目标输出

```cpp
// 同时输出到控制台和文件
auto log_file = std::make_shared<std::ofstream>("reactor.log", std::ios::app);

SrtReactor::set_log_callback([log_file](asrt::LogLevel level, const char* area, const char* message) {
    std::string log_line = std::string("[") + area + "] " + message;
    
    // 输出到控制台
    std::cout << log_line << std::endl;
    
    // 输出到文件
    *log_file << log_line << std::endl;
    log_file->flush();
});
```

---

## 🎯 使用场景

### 场景 1: 开发调试

```cpp
// 查看所有细节（包括 SRT 协议栈）
SrtReactor::set_log_level(asrt::LogLevel::Debug);

auto& reactor = SrtReactor::get_instance();
// 可以看到：
// - Reactor 的 socket 管理
// - SRT 的数据包发送/接收
// - 连接状态变化
// - 重传和拥塞控制
```

### 场景 2: 生产环境

```cpp
// 只记录重要信息
SrtReactor::set_log_level(asrt::LogLevel::Notice);

// 输出到日志文件
auto logger = setup_production_logger();
SrtReactor::set_log_callback([logger](auto level, auto area, auto msg) {
    logger->log(level, area, msg);
});
```

### 场景 3: 问题排查

```cpp
// 运行时动态调整日志级别
if (problem_detected) {
    // 临时开启详细日志
    SrtReactor::set_log_level(asrt::LogLevel::Debug);
    
    // ... 执行问题操作 ...
    
    // 恢复正常级别
    SrtReactor::set_log_level(asrt::LogLevel::Notice);
}
```

### 场景 4: 性能监控

```cpp
// 只记录错误，降低日志开销
SrtReactor::set_log_level(asrt::LogLevel::Error);

// 自定义回调发送到监控系统
SrtReactor::set_log_callback([](auto level, auto area, auto msg) {
    if (level >= asrt::LogLevel::Error) {
        send_to_monitoring_system(area, msg);
    }
});
```

---

## 📊 日志输出格式

### 默认格式

```
[级别] [区域] 消息
```

示例：
```
[INFO ] [Reactor] SrtReactor started
[DEBUG] [Reactor] Socket 123 added to epoll (events=0x9)
[DEBUG] [SRT] Sending packet seq=1234
[ERROR] [Reactor] Socket 123 error: Connection reset [events=0xd]
```

### 区域说明

| 区域 | 说明 |
|------|------|
| **Reactor** | `SrtReactor` 内部操作日志 |
| **SRT** | SRT 协议栈日志 |
| 其他 | SRT 库的其他功能区域 |

---

## ⚠️ 注意事项

### 1. 线程安全

- ✅ 所有 API 都是线程安全的
- ✅ 回调函数会在不同线程中被调用
- ⚠️ 确保回调函数本身是线程安全的

```cpp
// ✅ 正确：使用线程安全的操作
SrtReactor::set_log_callback([](auto level, auto area, auto msg) {
    static std::mutex mtx;
    std::lock_guard<std::mutex> lock(mtx);
    std::cout << msg << std::endl;
});

// ❌ 错误：非线程安全的操作
std::vector<std::string> logs; // 非线程安全
SrtReactor::set_log_callback([&logs](auto level, auto area, auto msg) {
    logs.push_back(msg); // 可能导致数据竞争！
});
```

### 2. 异常处理

- ⚠️ 回调函数不应抛出异常
- 如果必须处理异常，在回调内部捕获

```cpp
// ✅ 正确：内部处理异常
SrtReactor::set_log_callback([](auto level, auto area, auto msg) {
    try {
        risky_operation(msg);
    } catch (const std::exception& e) {
        std::cerr << "日志回调异常：" << e.what() << std::endl;
    }
});
```

### 3. 性能考虑

- 回调函数应尽快返回，避免阻塞
- 如果需要耗时操作，考虑使用异步队列

```cpp
// ✅ 推荐：使用队列异步处理
auto log_queue = std::make_shared<ThreadSafeQueue<std::string>>();

SrtReactor::set_log_callback([log_queue](auto level, auto area, auto msg) {
    // 快速入队，立即返回
    log_queue->push(std::string(msg));
});

// 在另一个线程中处理日志
std::thread log_processor([log_queue]() {
    while (true) {
        auto msg = log_queue->pop();
        // 耗时操作：写入数据库、发送网络等
        process_log(msg);
    }
});
```

### 4. 生命周期

- ✅ 回调中捕获的对象必须保持有效
- ✅ 使用 `shared_ptr` 管理生命周期

```cpp
// ✅ 正确：使用 shared_ptr
auto logger = std::make_shared<MyLogger>();
SrtReactor::set_log_callback([logger](auto level, auto area, auto msg) {
    logger->log(msg); // logger 生命周期由 shared_ptr 管理
});

// ❌ 错误：悬空引用
MyLogger logger;
SrtReactor::set_log_callback([&logger](auto level, auto area, auto msg) {
    logger.log(msg); // 如果 logger 被销毁，这里会崩溃！
});
```

---

## 🔍 常见问题

### Q1: 如何完全关闭日志？

A: 无法完全关闭（SRT 库的限制），但可以：

```cpp
// 方法 1：设置为只输出 Critical
SrtReactor::set_log_level(asrt::LogLevel::Critical);

// 方法 2：使用空回调（丢弃所有日志）
SrtReactor::set_log_callback([](auto, auto, auto) {
    // 不做任何事，丢弃日志
});
```

### Q2: 日志太多怎么办？

A: 提高日志级别或按区域过滤：

```cpp
// 方法 1：提高级别
SrtReactor::set_log_level(asrt::LogLevel::Error);

// 方法 2：过滤区域
SrtReactor::set_log_callback([](auto level, auto area, auto msg) {
    if (std::string(area) != "SRT") { // 只保留非 SRT 的日志
        std::cout << msg << std::endl;
    }
});
```

### Q3: 如何看到 SRT 协议栈的日志？

A: 设置为 Debug 级别即可：

```cpp
SrtReactor::set_log_level(asrt::LogLevel::Debug);
// 现在可以看到 SRT 的所有日志
```

### Q4: 可以动态切换日志级别吗？

A: 可以，任何时候都可以调整：

```cpp
// 运行时动态调整
if (debug_mode_enabled) {
    SrtReactor::set_log_level(asrt::LogLevel::Debug);
} else {
    SrtReactor::set_log_level(asrt::LogLevel::Notice);
}
```

### Q5: 如何集成到现有的日志系统？

A: 使用自定义回调：

```cpp
// 假设你有一个全局 logger
extern Logger* g_logger;

SrtReactor::set_log_callback([](auto level, auto area, auto msg) {
    // 转换并调用你的日志系统
    g_logger->log(convert_level(level), area, msg);
});
```

---

## 📊 性能对比

| 配置 | 性能影响 | 使用场景 |
|------|---------|---------|
| 默认输出 + Notice | 很低 | 生产环境（推荐） |
| 默认输出 + Debug | 中等 | 开发调试 |
| 自定义回调（简单） | 低 | 文件输出、简单处理 |
| 自定义回调（复杂） | 取决于实现 | 数据库、网络发送 |
| 空回调（丢弃日志） | 极低 | 性能测试 |

---

## 🎉 总结

增强的日志系统特点：

1. **简单** - 只需 3 个 API 函数
2. **灵活** - 支持自定义输出到任何地方
3. **统一** - Reactor 和 SRT 协议栈共用日志系统
4. **高效** - 线程安全，性能优秀

推荐配置：

```cpp
// 开发环境
SrtReactor::set_log_level(asrt::LogLevel::Debug);

// 生产环境
SrtReactor::set_log_level(asrt::LogLevel::Notice);
SrtReactor::set_log_callback([logger](auto level, auto area, auto msg) {
    logger->log(level, area, msg);
});

// 性能测试
SrtReactor::set_log_callback([](auto, auto, auto) {}); // 丢弃所有日志
```

完整示例请参考：[examples/custom_logging_example.cpp](../examples/custom_logging_example.cpp)




