# 错误码标准化 - 修改文件清单

## 📋 新增文件

### 1. `src/asrt/srt_error.h` ⭐ 核心新增
**功能**: SRT 错误码标准化系统

**内容**:
- `srt_errc` 枚举 - SRT 特定错误码定义
- `srt_category_impl` 类 - 实现 `std::error_category`
- `make_error_code()` 函数 - 从 `srt_errc` 创建 `std::error_code`
- `make_srt_error_code()` 函数 - 从 SRT 原生错误映射到标准错误码
- 与 `std::is_error_code_enum` 的特化

**关键特性**:
```cpp
// 自定义错误枚举
enum class srt_errc {
    success = 0,
    connection_setup = 1000,
    epoll_add_failed = 3001,
    timeout = 5000,
    // ...
};

// 映射到标准错误条件
std::error_condition default_error_condition(int ev) const noexcept override {
    switch (static_cast<srt_errc>(ev)) {
        case srt_errc::connection_lost:
            return std::errc::connection_reset;
        // ...
    }
}
```

### 2. `docs/ERROR_HANDLING.md`
**功能**: 错误处理完整指南

**章节**:
- 错误码类型概述
- 标准 ASIO 错误码
- SRT 特定错误码
- 错误处理示例
- 错误码转换表
- 与 ASIO 其他协议的兼容性
- 最佳实践

### 3. `docs/ERROR_CODE_REFACTORING.md`
**功能**: 重构过程和技术细节

**章节**:
- 重构目标
- 完成的工作详解
- 兼容性对比
- 测试结果
- 关键改进说明
- 使用建议

### 4. `ERROR_CODE_CHANGES.md` (本文件)
**功能**: 修改文件清单

## 🔧 修改文件

### 1. `src/asrt/srt_reactor.cpp`

#### 修改 1: 添加头文件
```cpp
#include "srt_error.h"
```

#### 修改 2: Epoll 添加失败错误
**位置**: 第 201-206 行
```cpp
// 之前
h_moved(std::make_error_code(std::errc::io_error), 0);

// 现在
h_moved(asrt::make_error_code(asrt::srt_errc::epoll_add_failed), 0);
```

#### 修改 3: Epoll 更新失败错误
**位置**: 第 215-219 行
```cpp
// 之前
h_moved(std::make_error_code(std::errc::io_error), 0);

// 现在
h_moved(asrt::make_error_code(asrt::srt_errc::epoll_update_failed), 0);
```

#### 修改 4: 超时错误处理 (async_wait_readable)
**位置**: 第 122-127 行
```cpp
// 之前
if (ec == asio::error::operation_aborted && timed_out->load()) {
    co_return -1;  // 返回 -1 表示超时
}

// 现在
if (ec == asio::error::operation_aborted && timed_out->load()) {
    // 超时使用标准的 timed_out 错误
    throw asio::system_error(std::make_error_code(std::errc::timed_out));
}
```

#### 修改 5: 超时错误处理 (async_wait_writable)
**位置**: 第 168-173 行
```cpp
// 同上，async_wait_writable 的超时处理
```

**影响**:
- ✅ 超时不再返回 -1，而是抛出标准异常
- ✅ 错误更明确，使用专用的错误码
- ✅ 与 ASIO 其他协议一致

### 2. `tests/test_srt_reactor.cpp`

#### 修改 1: TimeoutOnReadable 测试
**位置**: 第 450-500 行

**之前**:
```cpp
int result_value = 0;
result_value = co_await reactor_->async_wait_readable(server, 100ms);
EXPECT_EQ(result_value, -1) << "Should have timed out";
```

**现在**:
```cpp
bool timeout_occurred = false;
try {
    co_await reactor_->async_wait_readable(server, 100ms);
    ADD_FAILURE() << "Expected timeout exception";
} catch (const asio::system_error& e) {
    if (e.code() == std::make_error_code(std::errc::timed_out)) {
        timeout_occurred = true;
    }
}
EXPECT_TRUE(timeout_occurred);
```

#### 修改 2: WritableWithTimeout 测试
**位置**: 第 569-615 行

**之前**:
```cpp
result_value = co_await reactor_->async_wait_writable(client, 1000ms);
EXPECT_GE(result_value, 0);
```

**现在**:
```cpp
try {
    result_value = co_await reactor_->async_wait_writable(client, 1000ms);
    EXPECT_GE(result_value, 0);
} catch (const asio::system_error& e) {
    if (e.code() == std::make_error_code(std::errc::timed_out)) {
        ADD_FAILURE() << "Unexpected timeout on writable socket";
    }
    test_exception = std::current_exception();
}
```

**影响**:
- ✅ 测试现在验证异常而非返回值
- ✅ 增强了错误检查
- ✅ 消除了编译警告（未使用变量）

### 3. `README.md`

#### 修改位置: 第 26-47 行

**添加内容**: "错误处理" 章节

```markdown
### 错误处理

`asio_srt` 使用标准的 `std::error_code` 体系，与 ASIO 完全兼容：

- ✅ 使用 `asio::system_error` 异常
- ✅ 支持标准错误码（`std::errc`、`asio::error`）
- ✅ SRT 原生错误自动映射到标准错误
- ✅ 与 TCP/UDP 相同的错误处理模式

**示例**：
```cpp
try {
    co_await reactor.async_wait_readable(sock, 5000ms);
} catch (const asio::system_error& e) {
    if (e.code() == std::make_error_code(std::errc::timed_out)) {
        // 超时处理
    }
}
```

详见：[错误处理指南](docs/ERROR_HANDLING.md)
```

## 📊 统计信息

### 代码变更

| 文件 | 新增行 | 删除行 | 修改行 |
|------|--------|--------|--------|
| `src/asrt/srt_error.h` | 177 | 0 | 0 (新文件) |
| `src/asrt/srt_reactor.cpp` | 11 | 6 | 5 |
| `tests/test_srt_reactor.cpp` | 28 | 15 | 13 |
| `README.md` | 22 | 0 | 22 |
| **总计** | **238** | **21** | **40** |

### 文档变更

| 文档 | 行数 | 说明 |
|------|------|------|
| `docs/ERROR_HANDLING.md` | 425 | 完整错误处理指南 |
| `docs/ERROR_CODE_REFACTORING.md` | 482 | 重构技术细节 |
| `docs/TIMEOUT_IMPLEMENTATION_COMPARISON.md` | 305 | 超时实现对比 (之前创建) |
| `ERROR_CODE_CHANGES.md` | 本文档 | 修改清单 |

### 测试结果

```bash
[==========] 11 tests from 1 test suite ran. (894 ms total)
[  PASSED  ] 11 tests.
```

- ✅ 所有测试通过
- ✅ 无编译警告
- ✅ 无运行时错误

## 🎯 兼容性影响

### 破坏性变更

#### 1. 超时行为变更

**旧 API**:
```cpp
int result = co_await async_wait_readable(sock, 1000ms);
if (result == -1) {
    // 超时
}
```

**新 API**:
```cpp
try {
    int result = co_await async_wait_readable(sock, 1000ms);
    // 成功，result >= 0
} catch (const asio::system_error& e) {
    if (e.code() == std::make_error_code(std::errc::timed_out)) {
        // 超时
    }
}
```

**迁移建议**:
```cpp
// 如果需要保持旧行为，可以包装：
asio::awaitable<int> async_wait_readable_compat(
    SRTSOCKET sock, 
    std::chrono::milliseconds timeout
) {
    try {
        co_return co_await reactor.async_wait_readable(sock, timeout);
    } catch (const asio::system_error& e) {
        if (e.code() == std::make_error_code(std::errc::timed_out)) {
            co_return -1;
        }
        throw;
    }
}
```

### 非破坏性变更

#### 1. 无超时版本

```cpp
// 这些 API 没有变化
asio::awaitable<int> async_wait_readable(SRTSOCKET sock);
asio::awaitable<int> async_wait_writable(SRTSOCKET sock);
```

#### 2. 错误码分类

```cpp
// 现在可以更精确地判断错误类型
if (ec.category() == asrt::srt_category()) {
    // SRT 特定错误
} else if (ec.category() == std::generic_category()) {
    // 标准错误
}
```

## 🚀 使用建议

### 推荐做法

1. **使用异常处理超时**
   ```cpp
   try {
       co_await reactor.async_wait_readable(sock, 5s);
   } catch (const asio::system_error& e) {
       if (e.code() == std::make_error_code(std::errc::timed_out)) {
           // 处理超时
       }
   }
   ```

2. **统一错误处理**
   ```cpp
   try {
       // 多个操作
   } catch (const asio::system_error& e) {
       // 统一处理
       log_error(e.code(), "operation failed");
   }
   ```

3. **使用标准错误码**
   ```cpp
   // 优先使用 std::errc 和 asio::error
   if (ec == std::errc::connection_reset) { ... }
   
   // 只在必要时使用 asrt::srt_errc
   if (ec == asrt::make_error_code(asrt::srt_errc::epoll_add_failed)) { ... }
   ```

### 避免的做法

1. ❌ 检查返回值 -1
   ```cpp
   if (result == -1) { /* 不再有效 */ }
   ```

2. ❌ 直接比较枚举
   ```cpp
   if (e.code() == std::errc::timed_out) { /* 编译错误 */ }
   ```

3. ❌ 忽略异常
   ```cpp
   co_await reactor.async_wait_readable(sock, 1s);
   // 如果超时，异常会传播！
   ```

## 📚 相关文档

1. [错误处理指南](docs/ERROR_HANDLING.md) - 完整的使用指南
2. [重构技术细节](docs/ERROR_CODE_REFACTORING.md) - 实现细节
3. [超时实现对比](docs/TIMEOUT_IMPLEMENTATION_COMPARISON.md) - 超时机制说明
4. [README.md](README.md) - 项目概述

## ✅ 检查清单

验证错误码标准化是否成功：

- [x] 所有测试通过
- [x] 无编译警告
- [x] 创建了 `srt_error.h`
- [x] 更新了 `srt_reactor.cpp` 的错误码
- [x] 更新了测试以验证异常
- [x] 创建了完整的文档
- [x] README 包含错误处理说明
- [x] 超时使用标准的 `std::errc::timed_out`
- [x] Epoll 错误使用专用的错误码
- [x] 与 ASIO 其他协议兼容

## 🎉 总结

错误码标准化重构已**成功完成**！

**主要成果**:
1. ✅ 完全符合 C++ 标准错误处理惯例
2. ✅ 与 ASIO 其他协议（TCP/UDP）完全兼容
3. ✅ 提供了丰富的文档和示例
4. ✅ 所有测试通过，代码质量高

**下一步**:
- 在实际项目中应用和验证
- 收集用户反馈
- 根据需要扩展错误码定义


