# SrtReactor 单元测试

本目录包含 `SrtReactor` 类的单元测试。

## 测试框架

使用 **Google Test (gtest)** 框架，通过 CMake 的 `FetchContent` 自动下载。

## 测试用例

### 1. **SingletonAccess** - 单例访问测试
验证 `SrtReactor` 的单例模式是否正常工作。

### 2. **IoContextAvailable** - IO 上下文可用性测试
确保 ASIO 的 `io_context` 正常运行。

### 3. **SocketWritableAfterCreation** - Socket 可写性测试
验证新创建的 socket 在建立连接后是否可写。

### 4. **SendReceiveData** - 数据发送接收测试
测试通过 SRT socket 对发送和接收数据的完整流程。

### 5. **MultipleConcurrentOperations** - 并发操作测试
验证多个 socket 可以同时进行异步操作。

### 6. **OperationCancellation** - 操作取消测试
测试异步操作的取消机制是否正常工作。

### 7. **SimultaneousReadWriteOperations** - 同时读写测试
验证同一个 socket 可以同时进行读和写操作。

### 8. **InvalidSocketHandling** - 无效 Socket 处理测试
确保对无效 socket 的操作能够正确抛出异常。

## 运行测试

### 构建测试

```bash
# 在项目根目录
mkdir -p build && cd build
cmake ..
make

# 或者只构建测试
make test_srt_reactor
```

### 运行所有测试

```bash
# 使用 CTest
ctest --output-on-failure

# 或直接运行测试可执行文件
./tests/test_srt_reactor
```

### 运行特定测试

```bash
# 运行特定测试用例
./tests/test_srt_reactor --gtest_filter=SrtReactorTest.SendReceiveData

# 运行匹配模式的测试
./tests/test_srt_reactor --gtest_filter=*Concurrent*

# 列出所有测试
./tests/test_srt_reactor --gtest_list_tests
```

### 详细输出

```bash
# 显示详细信息
./tests/test_srt_reactor --gtest_print_time=1

# 重复运行测试（用于检测不稳定的测试）
./tests/test_srt_reactor --gtest_repeat=10
```

## 测试辅助函数

### `create_socket_pair()`

创建一对已连接的 SRT socket（客户端和服务器），用于测试数据传输。

**返回值**: `std::pair<SRTSOCKET, SRTSOCKET>` - (client, server)

## 注意事项

1. **非阻塞模式**: 所有测试 socket 都设置为非阻塞模式
2. **超时设置**: 每个测试都有 5 秒的超时限制，避免测试挂起
3. **资源清理**: `TearDown()` 会自动清理所有创建的 socket
4. **异常处理**: 使用 `std::exception_ptr` 在协程中捕获和重新抛出异常

## 添加新测试

创建新测试用例的模板：

```cpp
TEST_F(SrtReactorTest, YourTestName) {
    // 准备
    auto [client, server] = create_socket_pair();
    if (client == SRT_INVALID_SOCK || server == SRT_INVALID_SOCK) {
        GTEST_SKIP() << "Failed to create socket pair";
    }

    bool test_completed = false;
    std::exception_ptr test_exception;

    // 执行异步操作
    asio::co_spawn(
        reactor_->get_io_context(),
        [&]() -> asio::awaitable<void> {
            try {
                // 你的测试逻辑
                co_await reactor_->async_wait_writable(client);
                test_completed = true;
            } catch (...) {
                test_exception = std::current_exception();
            }
        },
        asio::detached
    );

    // 等待完成
    auto start = std::chrono::steady_clock::now();
    while (!test_completed && 
           std::chrono::steady_clock::now() - start < 5s) {
        std::this_thread::sleep_for(10ms);
    }

    // 断言
    if (test_exception) {
        std::rethrow_exception(test_exception);
    }
    EXPECT_TRUE(test_completed);
}
```

## 故障排除

### 测试超时
如果测试超时，可能是：
- SRT socket 连接失败
- 异步操作未正确完成
- Reactor 的 poll 循环未运行

### 段错误
检查：
- Socket 是否有效
- 是否在 socket 关闭后使用
- 线程安全问题

### 间歇性失败
- 增加超时时间
- 检查竞态条件
- 使用 `--gtest_repeat` 重复运行

## 持续集成

在 CI 环境中运行：

```bash
# 构建并运行测试
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
cd build && ctest --output-on-failure --verbose
```


