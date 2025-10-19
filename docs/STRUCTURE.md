# ASIO-SRT 项目结构

项目组织和目录结构的完整说明。

## 📂 目录树

```
asio_srt/
│
├── 📄 README.md                      # 项目主文档
├── 📄 CHANGELOG.md                   # 变更日志
├── 📄 LICENSE                        # 开源许可证
├── ⚙️  CMakeLists.txt                # CMake 构建配置
│
├── 📖 docs/                          # 📚 文档中心
│   ├── README.md                     # 文档索引（从这里开始！）
│   ├── STRUCTURE.md                  # 本文件
│   │
│   ├── 📘 guides/                    # 使用指南
│   │   ├── README.md
│   │   ├── QUICK_START.md            # 快速入门 ⭐
│   │   ├── SRT_SOCKET_GUIDE.md       # SRT Socket 指南
│   │   ├── CALLBACK_AND_OPTIONS_GUIDE.md
│   │   └── bcast/                    # 广播功能指南
│   │       ├── README.md
│   │       ├── BCAST_GUIDE.md
│   │       ├── BATCH_OPERATIONS.md
│   │       └── TIMEOUT_FEATURES.md
│   │
│   ├── 📗 api/                       # API 参考文档
│   │   ├── README.md
│   │   ├── acore/                    # ACORE 组件 API
│   │   │   ├── README.md             # ACORE 概览 ⭐
│   │   │   ├── ASYNC_PRIMITIVES.md   # 异步原语文档
│   │   │   ├── CANCELLATION_SUPPORT.md
│   │   │   ├── COROUTINE_ONLY.md
│   │   │   ├── WAITGROUP_USAGE.md
│   │   │   ├── ASYNC_EVENT_ANALYSIS.md
│   │   │   ├── ASYNC_EVENT_REFACTORED.md
│   │   │   ├── ASYNC_QUEUE_SIMPLIFICATION.md
│   │   │   └── ASYNC_SEMAPHORE_EXPLAINED.md
│   │   └── asrt/                     # (待添加)
│   │
│   ├── 🏗️  design/                   # 设计文档
│   │   ├── README.md
│   │   ├── error-handling/           # 错误处理设计 (7 文件)
│   │   │   ├── ERROR_HANDLING.md     # 概览 ⭐
│   │   │   ├── ERROR_CODE_REFACTORING.md
│   │   │   ├── ERROR_CODE_CHANGES.md
│   │   │   ├── ERROR_EVENT_IMPLEMENTATION.md
│   │   │   ├── ERROR_EVENT_CHANGES.md
│   │   │   ├── ERROR_NOTIFICATION_STRATEGY.md
│   │   │   └── ERROR_EVENT_HANDLING_ANALYSIS.md
│   │   ├── logging/                  # 日志系统设计 (2 文件)
│   │   │   ├── LOGGING_ENHANCED.md   # 概览 ⭐
│   │   │   └── LOGGING_ENHANCEMENT.md
│   │   ├── timeout/                  # 超时机制设计 (2 文件)
│   │   │   ├── TIMEOUT_API.md        # 概览 ⭐
│   │   │   └── TIMEOUT_IMPLEMENTATION_COMPARISON.md
│   │   ├── srt/                      # SRT 协议设计 (3 文件)
│   │   │   ├── SRT_NATIVE_CALLBACKS.md
│   │   │   ├── SRT_OPTIONS_V2.md
│   │   │   └── SRT_V2_FEATURES.md
│   │   ├── QUEUE_COMPARISON.md       # 队列实现对比
│   │   └── REORGANIZATION.md         # 重组方案
│   │
│   ├── 🔧 development/               # 开发文档
│   │   ├── README.md
│   │   ├── PROJECT_STATUS.md         # 项目状态 ⭐
│   │   ├── TEST_RESULTS.md           # 测试结果
│   │   ├── code-reviews/             # 代码审查 (12+ 文件)
│   │   │   ├── README.md
│   │   │   ├── FINAL_REPORT.md       # 最终报告 ⭐
│   │   │   ├── EXECUTIVE_SUMMARY.md
│   │   │   ├── ACORE_FULL_CODE_REVIEW.md
│   │   │   ├── FINAL_CODE_REVIEW_ALL_COMPONENTS.md
│   │   │   ├── CODE_REVIEW_SUMMARY.md
│   │   │   ├── CODE_REVIEW_IMPROVEMENTS.md
│   │   │   ├── COMPLETE_REVIEW_SUMMARY.md
│   │   │   ├── HONEST_CODE_REVIEW_REPORT.md
│   │   │   └── archive/              # 归档的审查文档
│   │   └── summaries/                # 开发总结 (8 文件)
│   │       ├── README.md
│   │       ├── NEW_FEATURES.md
│   │       ├── ENHANCEMENT_SUMMARY.md
│   │       ├── UPDATE_SUMMARY.md
│   │       ├── IMPLEMENTATION_SUMMARY.md
│   │       ├── IMPLEMENTATION_SUMMARY_V2.md
│   │       ├── SRT_IMPLEMENTATION_SUMMARY.md
│   │       ├── CHANGES_SUMMARY.md
│   │       └── CHANGELOG_DISPATCHER.md
│   │
│   └── 📦 archive/                   # 归档文档
│       └── DOCUMENTATION_REORGANIZATION.md
│
├── 💻 src/                           # 源代码
│   ├── acore/                        # 异步核心组件
│   │   ├── README.md                 # 组件说明
│   │   ├── async_semaphore.hpp       # 异步信号量
│   │   ├── async_queue.hpp           # 异步队列
│   │   ├── async_event.hpp           # 异步事件
│   │   ├── async_waitgroup.hpp       # WaitGroup
│   │   ├── dispatcher.hpp            # 调度器
│   │   ├── handler_traits.hpp        # 处理器萃取
│   │   ├── CMakeLists.txt
│   │   ├── test_*.cpp                # 测试程序
│   │   ├── build_all_tests.sh        # 构建脚本
│   │   └── run_all_tests.sh          # 测试脚本
│   │
│   ├── asrt/                         # SRT+ASIO 集成
│   │   ├── srt_reactor.hpp           # Reactor 接口
│   │   ├── srt_reactor.cpp           # Reactor 实现
│   │   ├── srt_socket.hpp            # Socket 封装
│   │   ├── srt_socket.cpp
│   │   ├── srt_acceptor.hpp          # Acceptor 封装
│   │   ├── srt_acceptor.cpp
│   │   ├── srt_socket_options.hpp    # Socket 选项
│   │   ├── srt_socket_options.cpp
│   │   ├── srt_error.hpp             # 错误定义
│   │   ├── srt_log.hpp               # 日志封装
│   │   └── CMakeLists.txt
│   │
│   ├── aentry/                       # 应用入口（待完成）
│   │   └── ...
│   │
│   └── CMakeLists.txt                # 源码 CMake
│
├── 📝 examples/                      # 示例代码
│   ├── acore/                        # ACORE 示例
│   │   └── ...
│   ├── srt_client_example.cpp        # SRT 客户端示例
│   ├── srt_server_example.cpp        # SRT 服务端示例
│   ├── srt_client_v2_example.cpp
│   ├── srt_server_v2_example.cpp
│   ├── srt_streaming_example.cpp     # 流媒体示例
│   ├── srt_resilient_client_example.cpp
│   ├── custom_logging_example.cpp    # 自定义日志示例
│   ├── option_test_example.cpp
│   └── CMakeLists.txt
│
├── 🧪 tests/                         # 单元测试
│   ├── README.md                     # 测试说明
│   ├── test_srt_reactor.cpp
│   └── CMakeLists.txt
│
├── 📦 3rdparty/                      # 第三方库
│   └── asio-1.36.0/                  # ASIO 头文件库
│       └── include/
│
├── 🔨 depends/                       # 依赖管理
│   ├── pkgs/                         # 依赖包
│   │   └── srt-1.5.4.tar.gz
│   ├── resolved/                     # 已编译的依赖库
│   │   └── srt/
│   ├── scripts/                      # 构建脚本
│   │   └── depends.cmake
│   └── build/                        # 依赖构建目录
│
└── 🏗️  build/                        # CMake 构建目录（自动生成）
    └── ...
```

## 📊 文档组织

### 文档分类原则

| 目录 | 用途 | 读者 |
|------|------|------|
| `docs/guides/` | 使用指南和教程 | 用户、开发者 |
| `docs/api/` | API 参考文档 | 开发者 |
| `docs/design/` | 设计决策和架构 | 架构师、贡献者 |
| `docs/development/` | 开发过程文档 | 维护者、贡献者 |
| `docs/archive/` | 历史文档 | 需要时查阅 |

### 源代码目录文档

- `src/acore/README.md` - ACORE 组件使用说明
- `tests/README.md` - 测试说明
- 仅保留必要的 README，避免文档泛滥

## 🎯 快速导航

### 新用户
1. [README.md](../README.md) - 项目主页
2. [docs/guides/QUICK_START.md](guides/QUICK_START.md) - 快速入门
3. [examples/](../examples/) - 查看示例

### API 查询
1. [docs/api/acore/README.md](api/acore/README.md) - ACORE API
2. [docs/api/acore/ASYNC_PRIMITIVES.md](api/acore/ASYNC_PRIMITIVES.md) - 详细 API

### 了解设计
1. [docs/design/README.md](design/README.md) - 设计文档索引
2. 选择感兴趣的主题（错误处理、日志、超时等）

### 贡献代码
1. [docs/development/code-reviews/](development/code-reviews/) - 代码质量标准
2. [docs/development/PROJECT_STATUS.md](development/PROJECT_STATUS.md) - 当前状态
3. [tests/](../tests/) - 运行测试

## 📈 统计信息

### 代码统计
- **源文件**: 20+ 个 (.hpp, .cpp)
- **示例**: 10+ 个
- **测试**: 10+ 个

### 文档统计
- **使用指南**: 8 篇
- **API 文档**: 9 篇
- **设计文档**: 16 篇
- **开发文档**: 19 篇
- **总计**: 52+ 篇 Markdown 文档

### 代码行数（估算）
- ACORE 组件: ~2000 行
- ASRT 封装: ~1500 行
- 示例代码: ~1000 行
- 测试代码: ~2000 行

## 🔧 构建系统

### CMake 层次结构

```
CMakeLists.txt (根)
├── src/CMakeLists.txt
│   ├── acore/CMakeLists.txt
│   └── asrt/CMakeLists.txt
├── examples/CMakeLists.txt
└── tests/CMakeLists.txt
```

### 依赖管理

使用自定义的依赖管理系统：
- `depends/scripts/depends.cmake` - 依赖解析
- `depends/pkgs/` - 源代码包
- `depends/resolved/` - 编译产物

## 🌟 最佳实践

### 文档维护
1. ✅ 添加新组件时，更新 API 文档
2. ✅ 设计变更时，更新设计文档
3. ✅ 新功能完成时，更新使用指南
4. ✅ 保持文档和代码同步

### 代码组织
1. ✅ 头文件库放在对应目录
2. ✅ 测试与源码在同一目录
3. ✅ 示例独立于源码
4. ✅ 保持目录结构清晰

### 依赖管理
1. ✅ 第三方库放在 `3rdparty/`
2. ✅ 构建依赖放在 `depends/`
3. ✅ 避免循环依赖
4. ✅ 明确依赖版本

## 🔗 相关链接

- [文档索引](README.md) - 完整文档列表
- [项目主页](../README.md) - 返回主页
- [快速入门](guides/QUICK_START.md) - 开始使用

---

**文档版本**: 2.0  
**上次更新**: 2025-10-18
