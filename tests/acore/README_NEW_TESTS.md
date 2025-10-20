# ACORE 新增测试说明

## 📁 目录结构

测试文件已整理到正确的位置：

```
tests/acore/
├── CMakeLists.txt                      # 测试构建配置
├── quick_test.sh                        # 快速测试脚本 ✨
│
├── 原有测试 (7 个)
│   ├── test_async_event.cpp
│   ├── test_async_queue.cpp
│   ├── test_async_semaphore.cpp
│   ├── test_dispatcher.cpp
│   ├── test_waitgroup.cpp
│   ├── test_waitgroup_race.cpp
│   └── test_cancellation.cpp
│
└── 新增测试 (6 个)
    ├── test_async_mutex.cpp              # 8 个测试用例
    ├── test_async_periodic_timer.cpp     # 9 个测试用例
    ├── test_async_rate_limiter.cpp       # 10 个测试用例
    ├── test_async_barrier.cpp            # 9 个测试用例
    ├── test_async_auto_reset_event.cpp   # 10 个测试用例
    └── test_async_latch.cpp              # 10 个测试用例
```

---

## 🚀 快速开始

### 方法 1：使用快速测试脚本（推荐）

```bash
cd /home/ubuntu/codes/cpp/asio_srt/tests/acore

# 快速验证（只运行 async_mutex）
./quick_test.sh

# 运行所有测试（可能需要几分钟）
./quick_test.sh --run-all
```

### 方法 2：使用 CMake + CTest

```bash
cd /home/ubuntu/codes/cpp/asio_srt
mkdir -p build && cd build

# 配置
cmake ..

# 编译所有测试
make

# 运行所有测试
ctest

# 或只运行 acore 测试
ctest -R "Async.*Tests"

# 查看详细输出
ctest -V -R "AsyncMutexTests"
```

### 方法 3：手动编译和运行

```bash
cd /home/ubuntu/codes/cpp/asio_srt/tests/acore

# 编译
g++ -std=c++20 -I../../3rdparty/asio-1.36.0/include -I../../src \
    test_async_mutex.cpp -o test_async_mutex -lpthread

# 运行
./test_async_mutex
```

---

## ✅ 测试状态

### 已验证通过
| 测试 | 状态 | 测试数 | 说明 |
|------|------|--------|------|
| **test_async_mutex** | ✅ 全部通过 | 8/8 | 已完整验证 |

### 编译成功（待运行）
| 测试 | 编译 | 测试数 | 预计耗时 |
|------|------|--------|---------|
| test_async_periodic_timer | ✅ | 9 | ~5秒 |
| test_async_rate_limiter | ✅ | 10 | ~10秒 |
| test_async_barrier | ✅ | 9 | ~3秒 |
| test_async_auto_reset_event | ✅ | 10 | ~2秒 |
| test_async_latch | ✅ | 10 | ~2秒 |

---

## 📊 测试覆盖

### 新增组件测试统计
- **总测试用例**: 56 个
- **代码行数**: ~2,367 行
- **覆盖场景**:
  - ✅ 基本功能测试
  - ✅ 并发场景测试
  - ✅ 边界条件测试
  - ✅ 错误处理测试
  - ✅ 压力测试（100+ workers）
  - ✅ 性能验证测试

### async_mutex 测试详情（已验证）
```
✅ Test 1: 基本锁定/解锁
✅ Test 2: 多协程竞争（1000 次迭代，无竞态）
✅ Test 3: FIFO 公平性
✅ Test 4: 超时锁定
✅ Test 5: 锁守卫移动语义
✅ Test 6: 手动 unlock
✅ Test 7: 压力测试（100000 次操作，吞吐量 103199 locks/sec）
✅ Test 8: 防止重复 unlock
```

---

## 🔧 单独运行某个测试

```bash
cd /home/ubuntu/codes/cpp/asio_srt/tests/acore

# 编译
g++ -std=c++20 -I../../3rdparty/asio-1.36.0/include -I../../src \
    test_async_barrier.cpp -o /tmp/test_async_barrier -lpthread

# 运行
/tmp/test_async_barrier
```

---

## 📝 测试用例说明

### 1. test_async_mutex.cpp
- 基本锁定/解锁
- RAII 守卫
- 并发竞争
- FIFO 公平性
- 超时锁定
- 移动语义
- 压力测试
- 重复 unlock 防护

### 2. test_async_periodic_timer.cpp
- 周期触发
- 停止/暂停/恢复
- 动态修改周期
- 多定时器并发
- 一次性定时器
- 取消操作
- 精度测试
- 重启测试

### 3. test_async_rate_limiter.cpp
- 基本速率限制
- 突发流量
- 可变令牌消耗
- 非阻塞 try_acquire
- 并发请求
- 动态修改速率
- 重置/停止
- 等待队列
- 精度验证

### 4. test_async_barrier.cpp
- 基本同步
- 多轮同步
- arrive/wait 分离
- 不同参与者数量
- arrive_and_drop
- 状态查询
- 重置
- 压力测试（50 workers × 100 rounds）
- 时序验证

### 5. test_async_auto_reset_event.cpp
- 单次通知
- 只唤醒一个等待者
- 批量通知
- 信号计数
- 非阻塞 try_wait
- 初始状态
- 重置
- 取消所有
- 任务队列模式
- vs 手动重置事件对比

### 6. test_async_latch.cpp
- 基本倒计数
- 批量倒计数
- 初始计数为 0
- 多个等待者
- arrive_and_wait
- try_wait
- 一次性使用
- 启动屏障模式
- vs waitgroup 对比
- 压力测试

---

## 🎯 下一步

### 推荐操作
1. ✅ 测试整理到 tests/acore - **已完成**
2. ✅ 编译验证 - **已完成（6/6）**
3. ⏭️ 运行所有测试 - `./quick_test.sh --run-all`
4. ⏭️ 集成到 CI/CD

### CMake 构建（标准方式）
```bash
cd /home/ubuntu/codes/cpp/asio_srt/build
cmake ..
make

# 运行所有 acore 测试
ctest -R "Async.*Tests" -V
```

---

## 📖 相关文档

- **组件文档**: `../../src/acore/README.md`
- **实现总结**: `../../src/acore/IMPLEMENTATION_SUMMARY.md`
- **API 文档**: `../../docs/api/acore/`

---

**更新日期**: 2025-10-19  
**状态**: ✅ 所有测试已整理，编译成功  
**验证**: async_mutex 全部通过

