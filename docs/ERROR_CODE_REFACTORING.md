# 错误码标准化重构总结

## 🎯 重构目标

将 `asio_srt` 的错误处理标准化，使其与 ASIO 和 C++ 标准库完全兼容。

## ✅ 完成的工作

### 1. 创建 SRT 错误类别系统

**新文件**: `src/asrt/srt_error.h`

#### 1.1 定义 SRT 特定错误码

```cpp
enum class srt_errc {
    success = 0,
    
    // 连接相关错误 (1000-1999)
    connection_setup = 1000,
    connection_rejected = 1001,
    connection_lost = 1002,
    
    // 资源相关错误 (2000-2999)
    resource_fail = 2000,
    thread_fail = 2001,
    
    // 操作相关错误 (3000-3999)
    invalid_socket = 3000,
    epoll_add_failed = 3001,
    epoll_update_failed = 3002,
    
    // 数据传输错误 (4000-4999)
    send_failed = 4000,
    recv_failed = 4001,
    
    // 超时 (5000+)
    timeout = 5000,
};
```

#### 1.2 实现错误类别

```cpp
class srt_category_impl : public std::error_category {
public:
    const char* name() const noexcept override;
    std::string message(int ev) const override;
    std::error_condition default_error_condition(int ev) const noexcept override;
};
```

**关键特性**：
- 实现 `default_error_condition` 映射到标准错误条件
- 例如：`srt_errc::connection_lost` → `std::errc::connection_reset`
- 使得 SRT 错误可以与标准错误进行比较

#### 1.3 SRT 原生错误映射

```cpp
inline std::error_code make_srt_error_code() noexcept {
    int srt_error = srt_getlasterror(nullptr);
    
    switch (srt_error) {
        case SRT_EINVSOCK:
            return make_error_code(srt_errc::invalid_socket);
        case SRT_ECONNSETUP:
            return make_error_code(srt_errc::connection_setup);
        case SRT_ECONNREJ:
            return make_error_code(srt_errc::connection_rejected);
        case SRT_ECONNLOST:
            return make_error_code(srt_errc::connection_lost);
        case SRT_EASYNCSND:
        case SRT_EASYNCRCV:
            return std::make_error_code(std::errc::operation_would_block);
        case SRT_ETIMEOUT:
            return make_error_code(srt_errc::timeout);
        default:
            return std::make_error_code(std::errc::io_error);
    }
}
```

### 2. 更新 SrtReactor 错误处理

**修改**: `src/asrt/srt_reactor.cpp`

#### 2.1 Epoll 操作错误

**之前**：
```cpp
h_moved(std::make_error_code(std::errc::io_error), 0);
```

**现在**：
```cpp
h_moved(asrt::make_error_code(asrt::srt_errc::epoll_add_failed), 0);
h_moved(asrt::make_error_code(asrt::srt_errc::epoll_update_failed), 0);
```

#### 2.2 超时错误

**之前**：
```cpp
if (ec == asio::error::operation_aborted && timed_out->load()) {
    co_return -1;  // 返回 -1 表示超时
}
```

**现在**：
```cpp
if (ec == asio::error::operation_aborted && timed_out->load()) {
    throw asio::system_error(std::make_error_code(std::errc::timed_out));
}
```

**改进**：
- ✅ 使用标准的 `std::errc::timed_out` 错误码
- ✅ 抛出异常而非返回特殊值
- ✅ 与 ASIO 其他协议保持一致

### 3. 更新测试代码

**修改**: `tests/test_srt_reactor.cpp`

#### 3.1 超时测试

**之前**：
```cpp
int result = co_await reactor->async_wait_readable(server, 100ms);
EXPECT_EQ(result, -1) << "Should have timed out";
```

**现在**：
```cpp
try {
    co_await reactor->async_wait_readable(server, 100ms);
    ADD_FAILURE() << "Expected timeout exception";
} catch (const asio::system_error& e) {
    if (e.code() == std::make_error_code(std::errc::timed_out)) {
        timeout_occurred = true;
    }
}
EXPECT_TRUE(timeout_occurred);
```

#### 3.2 错误码比较

**关键修正**：
```cpp
// ❌ 错误 - 直接比较枚举会编译失败
if (e.code() == std::errc::timed_out) { ... }

// ✅ 正确 - 使用 make_error_code
if (e.code() == std::make_error_code(std::errc::timed_out)) { ... }
```

### 4. 文档更新

创建了以下文档：

1. **`docs/ERROR_HANDLING.md`** - 完整的错误处理指南
   - 错误码类型说明
   - 使用示例
   - 与 ASIO 其他协议的兼容性
   - 最佳实践

2. **`README.md`** - 添加错误处理章节
   - 快速示例
   - 指向详细文档的链接

3. **`docs/ERROR_CODE_REFACTORING.md`** (本文档)
   - 重构总结

## 📊 兼容性对比

### 与 TCP 的对比

```cpp
// TCP (标准 ASIO)
asio::awaitable<void> tcp_read(asio::ip::tcp::socket& sock) {
    try {
        char buffer[1024];
        co_await sock.async_read_some(asio::buffer(buffer), asio::use_awaitable);
    } catch (const asio::system_error& e) {
        if (e.code() == asio::error::eof) { /* 连接关闭 */ }
        if (e.code() == asio::error::operation_aborted) { /* 取消 */ }
    }
}

// SRT (asio_srt) - 完全相同的模式！
asio::awaitable<void> srt_read(SRTSOCKET sock) {
    try {
        co_await reactor.async_wait_readable(sock);
        char buffer[1024];
        srt_recv(sock, buffer, sizeof(buffer));
    } catch (const asio::system_error& e) {
        if (e.code() == std::errc::connection_reset) { /* 连接关闭 */ }
        if (e.code() == asio::error::operation_aborted) { /* 取消 */ }
    }
}
```

### 错误映射表

| 场景 | TCP/UDP (ASIO) | SRT (asio_srt) |
|------|---------------|----------------|
| 连接关闭 | `asio::error::eof` | `std::errc::connection_reset` |
| 操作取消 | `asio::error::operation_aborted` | `asio::error::operation_aborted` |
| 超时 | `std::errc::timed_out` | `std::errc::timed_out` |
| 非阻塞 | `std::errc::operation_would_block` | `std::errc::operation_would_block` |

## 🎉 成果

### 编译结果

```bash
✅ 编译成功，无错误
⚠️  之前有 1 个警告（未使用变量），已修复
```

### 测试结果

```bash
[==========] 11 tests from 1 test suite ran. (849 ms total)
[  PASSED  ] 11 tests.
```

**所有测试通过**：
1. ✅ SingletonAccess
2. ✅ IoContextAvailable
3. ✅ SocketWritableAfterCreation
4. ✅ SendReceiveData
5. ✅ MultipleConcurrentOperations
6. ✅ OperationCancellation
7. ✅ SimultaneousReadWriteOperations
8. ✅ SocketCleanupAfterOperations
9. ✅ TimeoutOnReadable (更新为异常测试)
10. ✅ ReadableBeforeTimeout
11. ✅ WritableWithTimeout (增强错误检查)

## 🔑 关键改进

### 1. 类型安全

**之前**：
```cpp
int result = co_await async_wait_readable(sock, timeout);
if (result == -1) { /* 超时 */ }
else if (result > 0) { /* 成功 */ }
```
- ❌ 混淆返回值和错误状态
- ❌ 容易遗漏错误检查

**现在**：
```cpp
try {
    int events = co_await async_wait_readable(sock, timeout);
    // events 一定是有效的事件标志
} catch (const asio::system_error& e) {
    // 强制处理错误
}
```
- ✅ 返回值只表示成功的事件
- ✅ 异常强制错误处理

### 2. 标准化

- ✅ 使用 `std::error_code` 体系
- ✅ 遵循 C++11+ 错误处理惯例
- ✅ 与 ASIO 其他组件兼容
- ✅ 可与 Boost.System 互操作

### 3. 可扩展性

```cpp
// 自定义错误类别
namespace asrt {
    enum class srt_errc { /* ... */ };
    const std::error_category& srt_category();
    std::error_code make_error_code(srt_errc);
}

// 注册到 std
namespace std {
    template<>
    struct is_error_code_enum<asrt::srt_errc> : true_type {};
}
```

这使得：
- ✅ 可以添加新的 SRT 特定错误
- ✅ 自动映射到标准错误条件
- ✅ 支持跨类别的错误比较

### 4. 文档完善

- 📚 完整的错误处理指南
- 📚 与其他协议的对比示例
- 📚 最佳实践说明
- 📚 迁移指南（旧 API → 新 API）

## 🚀 使用建议

### 推荐模式

```cpp
asio::awaitable<void> recommended_pattern() {
    try {
        // 1. 使用超时等待
        co_await reactor.async_wait_readable(sock, 5s);
        
        // 2. 进行实际操作
        int n = srt_recv(sock, buffer, size);
        
        if (n < 0) {
            // 3. SRT 原生错误也可以转换
            throw asio::system_error(asrt::make_srt_error_code());
        }
        
    } catch (const asio::system_error& e) {
        // 4. 统一的错误处理
        if (e.code() == std::make_error_code(std::errc::timed_out)) {
            // 重试或报告超时
        } else if (e.code() == std::errc::connection_reset) {
            // 连接已断开
        } else {
            // 其他错误
            std::cerr << "Error: " << e.what() << "\n";
        }
    }
}
```

### 避免的模式

```cpp
// ❌ 不推荐 - 检查返回值
int result = co_await async_wait_readable(sock, timeout);
if (result == -1) { /* ... */ }

// ❌ 不推荐 - 直接比较枚举
if (e.code() == std::errc::timed_out) { /* 编译错误 */ }

// ❌ 不推荐 - 忽略错误
co_await async_wait_readable(sock); // 如果抛出异常会传播
```

## 📝 后续工作

### 可能的增强

1. **更丰富的错误信息**
   - 添加上下文信息（socket ID、操作类型）
   - 实现自定义异常类型（继承自 `asio::system_error`）

2. **错误恢复策略**
   - 提供辅助函数判断错误是否可恢复
   - 内置重试逻辑

3. **日志集成**
   - 与常用日志库集成
   - 提供结构化错误日志

4. **性能优化**
   - 错误路径的性能分析
   - 减少异常开销（在关键路径）

## 总结

通过这次重构，`asio_srt` 的错误处理：

- ✅ **标准化** - 完全符合 C++ 和 ASIO 惯例
- ✅ **类型安全** - 编译时错误检查
- ✅ **可互操作** - 与其他 ASIO 组件无缝集成
- ✅ **文档完善** - 清晰的使用指南
- ✅ **测试覆盖** - 所有测试通过

这使得 `asio_srt` 可以作为一个标准的 ASIO 扩展使用，降低了学习成本和集成难度！ 🎉


