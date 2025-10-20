# 🚀 从这里开始

欢迎来到 ASIO-SRT 项目！这是您的快速导航指南。

---

## 📍 您现在在哪里？

这是一个现代 C++ 项目，提供：
- ✨ **12 个异步组件**（互斥锁、定时器、限流器等）
- ✨ **SRT 协议的异步封装**（基于 ASIO）
- ✨ **C++20 协程支持**
- ✨ **完整的文档和示例**

---

## 🎯 我想...

### 快速开始使用
👉 **阅读**: [README.md](README.md)  
👉 **查看**: [docs/INDEX.md](docs/INDEX.md) - 完整文档索引  
👉 **运行**: 
```bash
cd tests/acore
./quick_test.sh
```

---

### 查看异步组件文档
👉 **完整 API**: [src/acore/README.md](src/acore/README.md) ⭐⭐⭐

**12 个组件**:
1. async_mutex - 互斥锁 🔒
2. async_periodic_timer - 定时器 ⏱️
3. async_rate_limiter - 限流器 🚦
4. async_barrier - 屏障 🚧
5. async_auto_reset_event - 事件 📢
6. async_latch - 计数器 🔢
7. async_semaphore - 信号量
8. async_queue - 队列
9. async_event - 事件
10. async_waitgroup - 等待组
11. dispatcher - 发布订阅
12. handler_traits - 工具

---

### 运行示例代码
👉 **位置**: [examples/](examples/)

```bash
# 编译
mkdir -p build && cd build
cmake .. && make

# 运行 ACORE 示例
./examples/acore/acore_mutex_example
./examples/acore/acore_timer_example
./examples/acore/acore_rate_limiter_example

# 运行 SRT 示例
./examples/srt_server_example
./examples/srt_client_example
```

---

### 运行测试
👉 **测试目录**: [tests/acore/](tests/acore/)

```bash
cd tests/acore

# 快速验证（1秒）
./quick_test.sh

# 运行所有测试（约30秒）
./quick_test.sh --run-all
```

---

### 查看文档
👉 **文档中心**: [docs/INDEX.md](docs/INDEX.md) ⭐

**快速链接**:
- [快速入门](docs/guides/QUICK_START.md)
- [API 文档](docs/api/acore/README.md)
- [设计文档](docs/design/README.md)
- [项目状态](docs/development/PROJECT_STATUS.md)

---

## 📊 项目概览

### 规模
- **代码量**: ~63,000 行
- **组件数**: 12 个（ACORE）
- **测试数**: 100+ 个
- **文档数**: 60+ 篇

### 技术
- C++20 (协程)
- ASIO 1.36.0
- SRT 1.5.4
- CMake 3.20+

### 质量
- ✅ 100% 编译成功
- ✅ 完整测试覆盖
- ✅ 详细文档
- ✅ 生产级代码

---

## 🎁 最新更新 (2025-10-19)

- ✨ 新增 6 个异步组件
- ✨ 新增 56 个测试用例
- ✨ 新增 4 个使用示例
- ✨ 完整的文档体系
- ✨ 项目结构重组

---

## 📖 推荐阅读顺序

### 第 1 天
1. ✅ 本文件（START_HERE.md）
2. ✅ [README.md](README.md) - 30 分钟
3. ✅ [src/acore/README.md](src/acore/README.md) - 1 小时
4. ✅ 运行示例 - 30 分钟

### 第 1 周
1. [docs/guides/QUICK_START.md](docs/guides/QUICK_START.md)
2. [docs/api/acore/](docs/api/acore/)
3. 运行所有测试
4. 阅读示例代码

### 第 1 月
1. 阅读设计文档
2. 阅读代码审查
3. 贡献代码

---

## 🚀 立即开始

### 5 分钟快速验证
```bash
# 1. 进入项目目录
cd /home/ubuntu/codes/cpp/asio_srt

# 2. 查看组件文档
cat src/acore/README.md | head -100

# 3. 运行快速测试
cd tests/acore && ./quick_test.sh

# 4. 运行一个示例
cd ../../build/examples/acore && ./acore_mutex_example
```

---

## 📞 获取帮助

### 文档
- 📖 [完整文档索引](docs/INDEX.md)
- 📖 [快速参考](docs/QUICK_REFERENCE.md)
- 📖 [项目结构](docs/STRUCTURE.md)

### 代码
- 💻 [源代码](src/)
- 🧪 [测试代码](tests/)
- 📝 [示例代码](examples/)

---

**欢迎使用 ASIO-SRT！** 🎉

👉 **下一步**: 运行 `cat docs/INDEX.md` 查看完整文档导航

