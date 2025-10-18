# 项目结构

```
asio_srt/
│
├── README.md                         # 项目主文档
├── CHANGELOG.md                      # 变更日志
├── LICENSE                           # 许可证
├── CMakeLists.txt                    # CMake 配置
│
├── 📖 docs/                          # 文档中心 ✨
│   ├── README.md                     # 📚 文档索引（从这里开始）
│   │
│   ├── api/                          # API 参考
│   │   ├── acore/                    # 异步核心组件 API
│   │   │   ├── ASYNC_PRIMITIVES.md
│   │   │   ├── CANCELLATION_SUPPORT.md
│   │   │   ├── WAITGROUP_USAGE.md
│   │   │   └── ...
│   │   └── asrt/                     # (待添加)
│   │
│   ├── guides/                       # 📘 使用指南
│   │   ├── QUICK_START.md            # 快速入门
│   │   ├── CALLBACK_AND_OPTIONS_GUIDE.md
│   │   ├── SRT_SOCKET_GUIDE.md
│   │   └── bcast/                    # 广播功能指南
│   │
│   ├── design/                       # 🏗️ 设计文档
│   │   ├── error-handling/           # 错误处理 (7 文件)
│   │   ├── logging/                  # 日志系统 (2 文件)
│   │   ├── timeout/                  # 超时机制 (2 文件)
│   │   └── srt/                      # SRT 协议 (3 文件)
│   │
│   └── development/                  # 🔧 开发文档
│       ├── PROJECT_STATUS.md
│       ├── TEST_RESULTS.md
│       ├── code-reviews/             # 代码审查 (12 报告)
│       └── summaries/                # 开发总结 (8 文件)
│
├── 💻 src/                           # 源代码
│   ├── acore/                        # 异步核心组件
│   │   ├── README.md                 # 组件说明
│   │   ├── async_semaphore.hpp
│   │   ├── async_queue.hpp
│   │   ├── async_event.hpp
│   │   ├── async_waitgroup.hpp
│   │   ├── dispatcher.hpp
│   │   └── handler_traits.hpp
│   │
│   ├── asrt/                         # SRT+ASIO 集成
│   │   ├── srt_reactor.hpp
│   │   ├── srt_reactor.cpp
│   │   ├── srt_socket.hpp
│   │   ├── srt_socket.cpp
│   │   └── ...
│   │
│   └── aentry/                       # 应用入口
│
├── 📝 examples/                      # 示例代码
│   ├── acore/
│   ├── srt_client_example.cpp
│   ├── srt_server_example.cpp
│   └── ...
│
├── 🧪 tests/                         # 测试代码
│   ├── README.md
│   └── ...
│
├── 📦 3rdparty/                      # 第三方库
│   └── asio-1.36.0/
│
└── 🔨 depends/                       # 依赖管理
    ├── pkgs/                         # 依赖包
    ├── resolved/                     # 已编译库
    └── scripts/                      # 构建脚本
```

## 🚀 快速导航

### 我想...

**开始使用项目**
- → [README.md](README.md)
- → [快速入门](docs/guides/QUICK_START.md)

**查看 API 文档**
- → [ACORE 组件](docs/api/acore/)
- → [源代码](src/acore/)

**了解设计决策**
- → [设计文档](docs/design/)
- → [错误处理](docs/design/error-handling/)
- → [日志系统](docs/design/logging/)

**参与开发**
- → [项目状态](docs/development/PROJECT_STATUS.md)
- → [代码审查](docs/development/code-reviews/)
- → [测试](tests/)

**查看示例**
- → [examples/](examples/)

## 📊 文档统计

- **总文档数**: 57 个 markdown 文件
- **API 文档**: 5 个
- **使用指南**: 13 个（含 bcast 子目录）
- **设计文档**: 14 个
- **开发文档**: 23 个（含审查和总结）
- **代码目录文档**: 2 个（src/acore/README.md, tests/README.md）

## ✨ 重组改进

✅ **根目录清爽** - 从 13 个文档减少到 2 个  
✅ **代码目录整洁** - src/acore 从 19 个文档减少到 1 个  
✅ **分类清晰** - 按类型组织（API、指南、设计、开发）  
✅ **易于导航** - 每个目录都有 README 索引  
✅ **链接更新** - 所有文档链接已更新  

详见：[文档重组说明](docs/DOCUMENTATION_REORGANIZATION.md)
