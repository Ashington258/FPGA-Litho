# FPGA-Litho 文档目录索引

> 创建日期: 2026-04-03
> 作者: K-Litho Team
> 目标: 整理文档结构，分类管理设计文档、测试报告和指导文档

---

## 📁 文档目录结构

```
doc/
├── design/          # 设计文档 (架构设计、方案分析)
│   ├── DATA_FLOW_ARCHITECTURE.md
│   ├── STORAGE_SOLUTION_COMPARISON.md
│   └── BRAM_ONLY_SOLUTION.md
│
├── reports/         # 测试报告 (HLS综合分析、性能验证)
│   ├── calc_image_csynth_analysis.md
│   ├── calc_image_integration_report.md
│   └── PHASE_SUMMARY_REPORT.md
│
├── guides/          # 指导文档 (项目概览、使用指南)
│   ├── BOARD_VERIFICATION_GUIDE.md
│   ├── PROJECT_SUMMARY.md
│   └── WORKSPACE_STRUCTURE.md
│
└── README.md        # 本文件 - 文档索引
```

---

## 📋 文档分类说明

### 1. **设计文档** (`design/`)

存储架构设计、方案分析和算法优化相关文档。

#### DATA_FLOW_ARCHITECTURE.md - 数据流架构详解
- **用途**: 详细说明FPGA-Litho数据流、AXI接口配置及具体参数
- **内容**:
  - TCC模式/SOCS模式数据流详解
  - 光源、掩模、TCC矩阵具体参数
  - AXI-Master/AXI-Lite接口配置
  - BRAM缓存架构和三阶段数据流
  - 完整执行时序分析
- **目标读者**: 系统集成工程师、硬件设计工程师

#### STORAGE_SOLUTION_COMPARISON.md - 存储方案比较分析
- **用途**: 分析不同FPGA存储方案，为硬件选择提供参考
- **内容**:
  - DDR/HBM方案 (高性能推荐)
  - BRAM-only方案 (资源受限器件)
  - URAM方案 (UltraScale+优化)
  - AXI-Stream方案 (流式处理)
  - 混合存储方案 (最佳平衡)
  - 硬件平台选择矩阵
- **目标读者**: 系统架构师、硬件选型决策者

#### BRAM_ONLY_SOLUTION.md - BRAM存储方案设计
- **用途**: xcku3p无DDR板卡的BRAM-only解决方案
- **内容**:
  - BRAM容量评估 (230KB可用)
  - TCC模式Nx≤3限制
  - SOCS模式完整支持
  - AXI-Lite接口设计
  - 数据加载/读取流程
- **目标读者**: 嵌入式工程师、原型验证工程师

---

### 2. **测试报告** (`reports/`)

存储HLS综合分析、性能验证和测试结果报告。

#### calc_image_csynth_analysis.md - calcImage模块综合分析
- **用途**: calcImage模块HLS综合性能分析和优化过程
- **内容**:
  - 原始版本综合结果 (38MHz瓶颈)
  - 根因分析 (浮点累加器链式依赖)
  - 优化策略 (累加器数组架构)
  - 性能对比 (38MHz → 273MHz)
- **目标读者**: HLS优化工程师、性能分析工程师

#### calc_image_integration_report.md - calcImage集成验证报告
- **用途**: calcImage模块系统集成和验证结果
- **内容**:
  - 集成测试结果 (3/3通过)
  - C仿真验证数据
  - HLS综合验证结果
  - 资源利用率统计
- **目标读者**: 验证工程师、系统集成工程师

#### PHASE_SUMMARY_REPORT.md - 开发阶段总结报告
- **用途**: 各开发阶段的完成情况和关键成果
- **内容**:
  - Phase 0-5完成情况
  - 关键技术突破
  - 性能指标达成
  - 遗留问题追踪
- **目标读者**: 项目经理、技术总监

---

### 3. **指导文档** (`guides/`)

存储项目概览、使用指南和配置说明文档。

#### BOARD_VERIFICATION_GUIDE.md - 板级验证指南
- **用途**: FPGA板卡验证流程和注意事项
- **内容**:
  - 硬件准备要求
  - Vivado集成步骤
  - XRT驱动安装
  - 性能测试方法
  - 常见问题排查
- **目标读者**: 硬件测试工程师、部署工程师

#### PROJECT_SUMMARY.md - 项目概览
- **用途**: FPGA-Litho项目整体介绍和技术亮点
- **内容**:
  - 项目目标和加速比
  - 核心算法介绍
  - 模块架构概览
  - 开发里程碑
  - 技术创新点
- **目标读者**: 新团队成员、技术评审人员

#### WORKSPACE_STRUCTURE.md - 工作空间结构说明
- **用途**: 项目目录结构和文件组织说明
- **内容**:
  - 工作空间布局
  - 目录功能说明
  - 关键文件定位
  - 文件命名规范
  - 版本管理建议
- **目标读者**: 所有团队成员

---

## 🔍 文档查找指南

### 按需求查找文档

#### 我想了解数据流架构？
→ 查看 `design/DATA_FLOW_ARCHITECTURE.md`
- 详细的数据流图示
- 具体工程参数说明
- AXI接口配置详解

#### 我想选择合适的存储方案？
→ 查看 `design/STORAGE_SOLUTION_COMPARISON.md`
- 5种存储方案对比
- 硬件平台选择矩阵
- 性能成本权衡分析

#### 我想分析HLS综合性能？
→ 查看 `reports/calc_image_csynth_analysis.md`
- 性能瓶颈分析
- 优化策略说明
- 综合结果对比

#### 我想开始板级验证？
→ 查看 `guides/BOARD_VERIFICATION_GUIDE.md`
- 完整验证流程
- 环境配置步骤
- 问题排查指南

#### 我想快速了解项目？
→ 查看 `guides/PROJECT_SUMMARY.md`
- 项目整体介绍
- 技术亮点总结
- 开发里程碑

---

## 📝 文档维护规范

### 新文档添加规则

1. **设计文档** (`design/`)
   - 文件命名: `模块名_ARCHITECTURE.md` 或 `方案名_SOLUTION.md`
   - 内容要求: 包含架构图、参数表、技术分析
   - 审核流程: 技术负责人审核

2. **测试报告** (`reports/`)
   - 文件命名: `模块名_TEST_REPORT.md` 或 `阶段名_ANALYSIS.md`
   - 内容要求: 包含测试数据、性能指标、验证结果
   - 审核流程: QA负责人审核

3. **指导文档** (`guides/`)
   - 文件命名: `功能名_GUIDE.md` 或 `主题名_SUMMARY.md`
   - 内容要求: 包含操作步骤、配置说明、使用示例
   - 审核流程: 文档负责人审核

### 文档更新流程

1. 创建新文档 → 添加到相应目录
2. 更新本索引文件 → 添加文档说明
3. 提交Git → 标注文档类型和用途
4. 通知团队 → 发送文档更新通知

---

## 📊 文档统计信息

| 分类 | 文档数量 | 总页数 | 更新频率 |
|------|---------|--------|----------|
| 设计文档 | 3 | ~30页 | 按需更新 |
| 测试报告 | 3 | ~20页 | 每阶段更新 |
| 指导文档 | 3 | ~15页 | 按版本更新 |

---

## 🔗 相关链接

- 项目仓库: `/root/project/FPGA/vitis/FPGA-Litho`
- TODO文档: `Vitis_HLS_Refactor_TODO.md`
- 主要代码: `src/`, `include/`
- 测试平台: `testbench/`
- HLS配置: `script/`

---

*本索引文件维护所有文档的分类和查找指南，方便团队成员快速定位所需文档*