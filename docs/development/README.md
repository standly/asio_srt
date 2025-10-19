# 开发文档

本目录包含项目开发过程中产生的各类文档、审查报告和开发总结。

## 📊 项目状态

| 文档 | 说明 |
|------|------|
| [PROJECT_STATUS.md](PROJECT_STATUS.md) | 项目当前状态和开发进展 ⭐ |
| [TEST_RESULTS.md](TEST_RESULTS.md) | 测试结果报告 |

## 🔍 代码审查报告

[code-reviews/](code-reviews/) 目录包含了详细的代码审查报告。

### 主要审查报告

| 报告 | 说明 | 重要性 |
|------|------|--------|
| [FINAL_REPORT.md](code-reviews/FINAL_REPORT.md) | 最终审查报告 | ⭐⭐⭐ |
| [EXECUTIVE_SUMMARY.md](code-reviews/EXECUTIVE_SUMMARY.md) | 执行摘要 | ⭐⭐⭐ |
| [ACORE_FULL_CODE_REVIEW.md](code-reviews/ACORE_FULL_CODE_REVIEW.md) | ACORE 完整审查 | ⭐⭐ |
| [FINAL_CODE_REVIEW_ALL_COMPONENTS.md](code-reviews/FINAL_CODE_REVIEW_ALL_COMPONENTS.md) | 所有组件审查 | ⭐⭐ |

### 其他审查文档
- [CODE_REVIEW_SUMMARY.md](code-reviews/CODE_REVIEW_SUMMARY.md) - 审查总结
- [CODE_REVIEW_IMPROVEMENTS.md](code-reviews/CODE_REVIEW_IMPROVEMENTS.md) - 改进建议
- [COMPLETE_REVIEW_SUMMARY.md](code-reviews/COMPLETE_REVIEW_SUMMARY.md) - 完整审查总结
- [HONEST_CODE_REVIEW_REPORT.md](code-reviews/HONEST_CODE_REVIEW_REPORT.md) - 诚实审查报告

### 归档的审查文档
[code-reviews/archive/](code-reviews/archive/) 包含次要的审查文档。

## 📝 开发总结

[summaries/](summaries/) 目录包含各个开发阶段的总结文档。

| 总结 | 说明 |
|------|------|
| [NEW_FEATURES.md](summaries/NEW_FEATURES.md) | 新功能说明 |
| [ENHANCEMENT_SUMMARY.md](summaries/ENHANCEMENT_SUMMARY.md) | 功能增强总结 |
| [UPDATE_SUMMARY.md](summaries/UPDATE_SUMMARY.md) | 更新总结 |
| [IMPLEMENTATION_SUMMARY.md](summaries/IMPLEMENTATION_SUMMARY.md) | 实现总结 V1 |
| [IMPLEMENTATION_SUMMARY_V2.md](summaries/IMPLEMENTATION_SUMMARY_V2.md) | 实现总结 V2 |
| [SRT_IMPLEMENTATION_SUMMARY.md](summaries/SRT_IMPLEMENTATION_SUMMARY.md) | SRT 实现总结 |
| [CHANGES_SUMMARY.md](summaries/CHANGES_SUMMARY.md) | 变更总结 |
| [CHANGELOG_DISPATCHER.md](summaries/CHANGELOG_DISPATCHER.md) | Dispatcher 变更日志 |

## 🎯 如何使用这些文档

### 了解项目质量
→ 阅读 [FINAL_REPORT.md](code-reviews/FINAL_REPORT.md) 和 [EXECUTIVE_SUMMARY.md](code-reviews/EXECUTIVE_SUMMARY.md)

### 了解开发历程
→ 查看 [summaries/](summaries/) 目录下的各类总结

### 贡献代码前
→ 阅读代码审查报告，了解代码质量标准

### 了解测试覆盖
→ 查看 [TEST_RESULTS.md](TEST_RESULTS.md)

## 📈 开发统计

- **代码审查报告**: 12 份（含归档）
- **开发总结**: 8 份
- **项目状态文档**: 2 份

## 🔧 开发工具

### 测试
```bash
cd tests/
./run_all_tests.sh
```

### ACORE 组件测试
```bash
cd src/acore/
./build_all_tests.sh
./run_all_tests.sh
```

### 构建
```bash
mkdir build && cd build
cmake ..
make
```

## 🔗 相关链接

- [项目状态](PROJECT_STATUS.md) - 当前进展
- [代码审查](code-reviews/) - 质量报告
- [开发总结](summaries/) - 开发历程
- [返回文档首页](../README.md) - 文档索引
