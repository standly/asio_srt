# 项目目录结构快速参考

## 📁 完整目录树

```
asio_srt/
│
├── src/                    # ✅ 源代码（纯净）
│   ├── acore/              # 异步核心组件
│   ├── asrt/               # SRT 集成模块
│   └── aentry/             # 应用入口
│
├── tests/                  # ✅ 测试（按模块分类）
│   ├── acore/              # acore 测试 (7个)
│   └── asrt/               # asrt 测试 (5个)
│
├── examples/               # ✅ 示例（按模块分类）
│   ├── acore/              # acore 示例 (12个)
│   └── *.cpp               # SRT 示例
│
├── docs/                   # ✅ 文档（按类型分类）
│   ├── api/                # API 文档
│   ├── guides/             # 使用指南
│   ├── design/             # 设计文档
│   └── development/        # 开发文档
│       └── acore/          # acore 开发文档
│
├── depends/                # 依赖管理
└── 3rdparty/               # 第三方库
```

## 🎯 查找指南

| 我想... | 查看这里 |
|---------|----------|
| 使用 acore 组件 | `src/acore/*.hpp` |
| 使用 SRT 集成 | `src/asrt/*.hpp`, `src/asrt/*.cpp` |
| 学习 acore 用法 | `examples/acore/` |
| 学习 SRT 用法 | `examples/srt_*.cpp` |
| 测试 acore | `tests/acore/` |
| 测试 SRT | `tests/asrt/` |
| 查看 API 文档 | `docs/api/` |
| 查看设计文档 | `docs/design/` |
| 查看开发文档 | `docs/development/` |

## 🔨 常用命令

```bash
# 重新构建
cmake -B build && cd build && make

# 构建特定模块测试
make test_async_queue       # acore
make test_srt_reactor       # asrt

# 运行测试
./tests/acore/test_async_queue
./tests/asrt/test_srt_reactor

# 运行所有测试
ctest --output-on-failure
```

## 📊 模块统计

| 模块 | 源文件 | 测试 | 示例 |
|------|--------|------|------|
| acore | 7个头文件 | 7个 | 12个 |
| asrt | 11个文件 | 5个 | 8个 |

## ✨ 整理亮点

- ✅ src/ 只包含源代码
- ✅ tests/ 按模块分类
- ✅ examples/ 按模块分类
- ✅ docs/ 按类型分类
- ✅ CMake 配置模块化
- ✅ 无构建产物混入

整理完成日期：2025-10-19
