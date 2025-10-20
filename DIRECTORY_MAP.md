# 📂 ASIO-SRT 目录导航图

快速找到您需要的文件！

---

## 🏠 根目录文件

| 文件 | 说明 | 重要性 |
|------|------|--------|
| **START_HERE.md** | 👈 从这里开始！ | ⭐⭐⭐ |
| README.md | 项目主页 | ⭐⭐⭐ |
| CHANGELOG.md | 变更历史 | ⭐⭐ |
| LICENSE | MIT 许可证 | ⭐ |
| CMakeLists.txt | 构建配置 | ⭐⭐ |
| FINAL_PROJECT_SUMMARY.md | 项目总结 | ⭐⭐ |
| ORGANIZATION_COMPLETE.md | 整理报告 | ⭐ |

---

## 📁 src/ - 源代码

```
src/
├── acore/               # ⭐⭐⭐ 异步组件库（12 个组件）
│   ├── README.md       # 完整 API 文档
│   ├── async_mutex.hpp
│   ├── async_periodic_timer.hpp
│   ├── async_rate_limiter.hpp
│   └── ... (其他组件)
│
├── asrt/                # SRT+ASIO 集成
│   ├── srt_reactor.*
│   ├── srt_socket.*
│   └── ...
│
└── aentry/              # 应用入口（待开发）
```

**重要文件**:
- `src/acore/README.md` - ACORE 完整文档 ⭐⭐⭐

---

## 🧪 tests/ - 测试代码

```
tests/
├── acore/               # ⭐⭐ ACORE 测试（13 个）
│   ├── quick_test.sh   # 快速测试脚本
│   ├── README_NEW_TESTS.md
│   ├── test_async_mutex.cpp
│   ├── test_async_periodic_timer.cpp
│   ├── test_async_rate_limiter.cpp
│   └── ... (其他测试)
│
└── asrt/                # ASRT 测试
```

**重要文件**:
- `tests/acore/quick_test.sh` - 快速测试脚本 ⭐⭐⭐
- `tests/acore/README_NEW_TESTS.md` - 测试说明 ⭐⭐

---

## 📝 examples/ - 示例代码

```
examples/
├── acore/               # ⭐⭐ ACORE 示例（16 个）
│   ├── mutex_example.cpp
│   ├── timer_example.cpp
│   ├── rate_limiter_example.cpp
│   ├── barrier_latch_example.cpp
│   ├── example.cpp
│   ├── advanced_example.cpp
│   └── ...
│
└── srt_*.cpp            # SRT 应用示例（9 个）
```

**重要文件**:
- `examples/acore/mutex_example.cpp` - 互斥锁示例 ⭐⭐
- `examples/acore/timer_example.cpp` - 定时器示例 ⭐⭐

---

## 📖 docs/ - 文档目录

```
docs/
├── INDEX.md             # ⭐⭐⭐ 完整文档索引
├── README.md            # 文档中心
├── QUICK_REFERENCE.md   # 快速参考
├── STRUCTURE.md         # 项目结构
│
├── guides/              # 使用指南（8+ 篇）
│   ├── QUICK_START.md
│   └── ...
│
├── api/                 # API 文档（10+ 篇）
│   └── acore/
│
├── design/              # 设计文档（15+ 篇）
│   ├── error-handling/
│   ├── logging/
│   ├── timeout/
│   └── srt/
│
├── development/         # 开发文档（20+ 篇）
│   ├── PROJECT_STATUS.md
│   ├── acore/          # ⭐ ACORE 开发文档
│   │   └── IMPLEMENTATION_SUMMARY.md
│   ├── reorganization/ # 重组文档
│   ├── code-reviews/
│   └── summaries/
│
└── archive/             # 归档文档
```

**重要文件**:
- `docs/INDEX.md` - 文档总索引 ⭐⭐⭐
- `docs/QUICK_REFERENCE.md` - 快速参考 ⭐⭐
- `docs/development/PROJECT_STATUS.md` - 项目状态 ⭐⭐

---

## 🔍 快速查找

### 我想了解...

| 目标 | 文件路径 |
|------|---------|
| **如何开始** | START_HERE.md → README.md → docs/INDEX.md |
| **ACORE API** | src/acore/README.md |
| **运行测试** | tests/acore/quick_test.sh |
| **查看示例** | examples/acore/*.cpp |
| **项目状态** | docs/development/PROJECT_STATUS.md |
| **设计决策** | docs/design/ |
| **代码审查** | docs/development/code-reviews/ |

---

## 💡 常用命令

### 文档
```bash
# 文档索引
cat docs/INDEX.md

# ACORE 组件文档
cat src/acore/README.md

# 快速参考
cat docs/QUICK_REFERENCE.md
```

### 编译和测试
```bash
# 编译
mkdir -p build && cd build && cmake .. && make

# 测试
cd ../tests/acore && ./quick_test.sh
```

### 运行示例
```bash
cd build/examples/acore
./acore_mutex_example
./acore_timer_example
```

---

## 📊 项目统计

- **组件数**: 12 个异步组件
- **代码量**: ~63,000 行
- **测试数**: 100+ 个
- **文档数**: 60+ 篇
- **示例数**: 25+ 个

---

## 🎯 推荐路径

### 5 分钟了解
1. 阅读本文件
2. 运行 `cat docs/INDEX.md`
3. 查看 `src/acore/README.md` 前 100 行

### 30 分钟上手
1. 阅读 README.md
2. 运行 quick_test.sh
3. 编译并运行一个示例

### 1 小时精通
1. 阅读完整 ACORE 文档
2. 运行所有示例
3. 查看测试代码

---

**下一步**: 👉 打开 [README.md](README.md)

🎉 **祝您使用愉快！** 🎉
