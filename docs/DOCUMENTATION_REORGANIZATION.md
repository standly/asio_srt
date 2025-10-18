# 文档重组说明

**日期**: 2025-10-18

## 📋 重组目标

原项目存在以下问题：
- 根目录有 13 个文档文件，过于杂乱
- `src/acore/` 目录混合了 19 个文档和代码文件
- 文档缺乏层次结构，难以查找
- 文档类型混杂（API、指南、设计、审查报告等）

## 🎯 重组原则

1. **代码与文档分离** - 源代码目录只保留代码和必要的 README
2. **分类组织** - 按文档类型分类（API、指南、设计、开发）
3. **层次清晰** - 使用子目录组织相关文档
4. **易于导航** - 每个目录都有 README 索引

## 📂 新的文档结构

```
docs/
├── README.md                         # 主文档索引
├── api/                              # API 参考文档
│   ├── README.md
│   └── acore/                        # ACORE 组件 API
│       ├── README.md                 # (从 src/acore/ 移动)
│       ├── ASYNC_PRIMITIVES.md
│       ├── COROUTINE_ONLY.md
│       ├── CANCELLATION_SUPPORT.md
│       └── WAITGROUP_USAGE.md
├── guides/                           # 使用指南
│   ├── README.md
│   ├── QUICK_START.md               # (从根目录移动)
│   ├── CALLBACK_AND_OPTIONS_GUIDE.md
│   ├── SRT_SOCKET_GUIDE.md
│   └── bcast/                       # 广播功能指南
│       ├── README.md
│       ├── BCAST_GUIDE.md
│       └── ... (共 9 个文件)
├── design/                           # 设计文档
│   ├── README.md
│   ├── error-handling/              # 错误处理设计
│   │   ├── ERROR_HANDLING.md
│   │   ├── ERROR_CODE_REFACTORING.md
│   │   ├── ERROR_CODE_CHANGES.md    # (从根目录移动)
│   │   ├── ERROR_EVENT_CHANGES.md   # (从根目录移动)
│   │   └── ... (共 7 个文件)
│   ├── logging/                     # 日志系统设计
│   │   ├── LOGGING_ENHANCED.md
│   │   └── LOGGING_ENHANCEMENT.md   # (从根目录移动)
│   ├── timeout/                     # 超时机制设计
│   │   ├── TIMEOUT_API.md
│   │   └── TIMEOUT_IMPLEMENTATION_COMPARISON.md
│   └── srt/                         # SRT 协议设计
│       ├── SRT_NATIVE_CALLBACKS.md
│       ├── SRT_OPTIONS_V2.md
│       └── SRT_V2_FEATURES.md
└── development/                      # 开发文档
    ├── README.md
    ├── PROJECT_STATUS.md            # (从根目录移动)
    ├── TEST_RESULTS.md              # (从根目录移动)
    ├── code-reviews/                # 代码审查报告 (12 个文件)
    │   ├── ACORE_FULL_CODE_REVIEW.md
    │   ├── FINAL_CODE_REVIEW_ALL_COMPONENTS.md
    │   └── ... (从 src/acore/ 移动)
    └── summaries/                   # 开发总结 (8 个文件)
        ├── NEW_FEATURES.md          # (从根目录移动)
        ├── ENHANCEMENT_SUMMARY.md   # (从根目录移动)
        ├── IMPLEMENTATION_SUMMARY.md
        └── ...
```

## 📊 文档统计

### 移动前
- **根目录**: 13 个 .md 文件（不含 README、CHANGELOG、LICENSE）
- **src/acore/**: 19 个 .md 文件
- **docs/**: 14 个 .md 文件（包含 bcast 子目录）
- **总计**: 57 个 markdown 文件

### 移动后
- **根目录**: 2 个 .md 文件（README.md、CHANGELOG.md）✅
- **src/acore/**: 1 个 .md 文件（README.md）✅
- **docs/**: 57 个 .md 文件（包括新增的索引文件）✅

## 🔄 主要变更

### 根目录清理
移走的文档：
- `QUICK_START.md` → `docs/guides/`
- `CALLBACK_AND_OPTIONS_GUIDE.md` → `docs/guides/`
- `ERROR_CODE_CHANGES.md` → `docs/design/error-handling/`
- `ERROR_EVENT_CHANGES.md` → `docs/design/error-handling/`
- `LOGGING_ENHANCEMENT.md` → `docs/design/logging/`
- `NEW_FEATURES.md` → `docs/development/summaries/`
- `ENHANCEMENT_SUMMARY.md` → `docs/development/summaries/`
- `UPDATE_SUMMARY.md` → `docs/development/summaries/`
- `IMPLEMENTATION_SUMMARY.md` → `docs/development/summaries/`
- `IMPLEMENTATION_SUMMARY_V2.md` → `docs/development/summaries/`
- `SRT_IMPLEMENTATION_SUMMARY.md` → `docs/development/summaries/`
- `PROJECT_STATUS.md` → `docs/development/`
- `TEST_RESULTS.md` → `docs/development/`

### src/acore/ 清理
移走的文档（18 个）：
- **API 文档** (5 个) → `docs/api/acore/`
  - `README.md`、`ASYNC_PRIMITIVES.md`、`COROUTINE_ONLY.md`
  - `CANCELLATION_SUPPORT.md`、`WAITGROUP_USAGE.md`
- **代码审查** (12 个) → `docs/development/code-reviews/`
- **开发总结** (1 个) → `docs/development/summaries/`

### 新建的索引文件
- `docs/README.md` - 主文档索引
- `docs/api/README.md` - API 文档索引
- `docs/guides/README.md` - 使用指南索引
- `docs/design/README.md` - 设计文档索引
- `docs/development/README.md` - 开发文档索引
- `src/acore/README.md` - ACORE 组件说明（重写）

## ✅ 改进效果

### 代码目录更清晰
- `src/acore/` 现在只有 1 个简洁的 README.md 和源代码
- 开发者可以专注于代码，不被大量文档干扰

### 文档易于查找
- 按类型分类：API、指南、设计、开发
- 每个类别都有明确的索引
- 相关文档集中在一起

### 层次结构合理
- 错误处理相关的 7 个文档集中在 `design/error-handling/`
- 代码审查的 12 个报告集中在 `development/code-reviews/`
- 广播功能的 9 个文档保持在 `guides/bcast/`

### 导航体验优化
- 从主 README 可以直接跳转到文档索引
- 每个子目录都有 README 引导
- 文档链接已全部更新

## 🔗 快速导航

新手从这里开始：
1. [文档主索引](README.md)
2. [快速入门指南](guides/QUICK_START.md)
3. [ACORE API 文档](api/acore/)

查找设计文档：
- [错误处理设计](design/error-handling/)
- [日志系统设计](design/logging/)
- [SRT 协议设计](design/srt/)

开发相关：
- [代码审查报告](development/code-reviews/)
- [项目状态](development/PROJECT_STATUS.md)
- [测试结果](development/TEST_RESULTS.md)

## 📝 维护建议

### 添加新文档时
1. **API 文档** → `docs/api/{component}/`
2. **使用指南** → `docs/guides/`
3. **设计决策** → `docs/design/{topic}/`
4. **代码审查** → `docs/development/code-reviews/`
5. **开发总结** → `docs/development/summaries/`

### 更新索引
添加新文档后，记得更新相应目录的 README.md 索引。

### 避免混乱
- ❌ 不要在根目录添加新的 .md 文件（除非是项目级的重要文档）
- ❌ 不要在源代码目录添加大量文档
- ✅ 使用合适的分类目录
- ✅ 保持文档命名清晰一致

