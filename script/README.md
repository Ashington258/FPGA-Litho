# Script 目录分类说明

本文档对 `script/` 目录下的脚本按功能进行分类整理。

---

## 目录结构（已重组）

```
script/
├── hls/              # HLS综合与仿真脚本 (12个文件)
├── verify/           # 板级验证脚本 (10个文件)
├── build/            # 构建与编译脚本 (3个文件)
├── config/           # 配置文件 (9个文件)
└── README.md         # 本文档
```

---

## 一、HLS综合与仿真脚本 (`hls/`)

### 1.1 HLS综合脚本

| 文件 | 功能 | 用途 |
|------|------|------|
| `csynth_standalone.tcl` | 独立C综合脚本 | Vivado HLS综合 |
| `run_csynth.tcl` | 主综合脚本 | 启动综合流程 |
| `run_csynth.py` | Python综合包装 | 自动化综合 |
| `run_csynth_bram.py` | BRAM版综合脚本 | Python控制 |
| `run_csynth_bram.tcl` | BRAM版TCL综合 | TCL控制 |
| `run_csynth_calc_image_integrated.tcl` | calc_image综合 | 单模块综合 |
| `run_csynth_socs.tcl` | SOCS模块综合 | 单模块综合 |
| `run_csynth_system.tcl` | 系统级综合 | 顶层综合 |

**使用示例**:
```bash
# 综合BRAM版本
vivado_hls -f script/hls/run_csynth_bram.tcl

# 或使用Python
python script/hls/run_csynth_bram.py
```

### 1.2 Co-Simulation脚本

| 文件 | 功能 | 用途 |
|------|------|------|
| `run_cosim_bram.tcl` | BRAM版协同仿真 | RTL验证 |
| `run_cosim_system.tcl` | 系统级协同仿真 | 系统验证 |

**使用示例**:
```bash
vivado_hls -f script/hls/run_cosim_bram.tcl
```

### 1.3 IP打包脚本

| 文件 | 功能 | 用途 |
|------|------|------|
| `run_package_bram.tcl` | BRAM版IP导出 | Vivado IP Catalog |
| `run_package_system.tcl` | 系统版IP导出 | Vivado IP Catalog |

**使用示例**:
```bash
vivado_hls -f script/hls/run_package_bram.tcl
```

---

## 二、板级验证脚本 (`verify/`)

### 2.1 Vivado Hardware Manager脚本（推荐）

| 文件 | 功能 | 平台 |
|------|------|------|
| `board_verify_complete.tcl` | ✅ **完整功能验证** | Vivado HM TCL |
| `board_verify_hardware_manager.tcl` | Hardware Manager脚本 | Vivado HM TCL |
| `board_verify_vivado.tcl` | Vivado TCL模式 | Vivado TCL |
| `bram_full_test.tcl` | BRAM完整测试 | Vivado HM TCL |
| `manual_test.tcl` | 手动测试命令 | Vivado HM TCL |
| `test_commands.tcl` | 分步测试命令 | Vivado HM TCL |
| `board_verify_quick.tcl` | 快速验证脚本 | Vivado HM TCL |

**推荐使用**:
```tcl
# 在 Vivado Hardware Manager TCL Console 中:
source script/verify/board_verify_complete.tcl  ;# 完整验证
source script/verify/manual_test.tcl            ;# 手动测试
```

### 2.2 其他调试工具脚本

| 文件 | 功能 | 平台 |
|------|------|------|
| `board_verify_xsct.tcl` | XSCT验证脚本 | XSCT |
| `board_verify_xsdb.tcl` | XSDB调试脚本 | XSDB |
| `board_verify.tcl` | XSCT基础脚本 | XSCT |

**使用示例**:
```bash
xsct
source script/verify/board_verify_xsct.tcl
```

---

## 三、构建与编译脚本 (`build/`)

| 文件 | 功能 | 用途 |
|------|------|------|
| `build_xclbin.py` | xclbin编译脚本 | Vitis v++编译 |
| `build_xclbin.sh` | xclbin编译Shell | Vitis v++编译 |
| `compile_standalone_test.py` | 独立测试编译 | 测试程序编译 |

**使用示例**:
```bash
python script/build/build_xclbin.py
./script/build/build_xclbin.sh
```

---

## 四、配置文件 (`config/`)

### 4.1 HLS配置文件

| 文件 | 功能 | 用途 |
|------|------|------|
| `hls_config.cfg` | 基础HLS配置 | 通用配置 |
| `hls_config_bram.cfg` | BRAM版配置 | BRAM优化 |
| `hls_config_bram_simple.cfg` | BRAM简化配置 | 快速测试 |
| `hls_config_bram_vitis.cfg` | BRAM Vitis配置 | Vitis流程 |
| `hls_config_full.cfg` | 完整流程配置 | 全流程 |
| `hls_config_system.cfg` | 系统级配置 | 系统集成 |
| `hls_integrated.cfg` | 集成版配置 | calc_image |
| `hls_socs.cfg` | SOCS配置 | SOCS模块 |

### 4.2 Vitis配置文件

| 文件 | 功能 | 用途 |
|------|------|------|
| `v++_config.ini` | v++编译配置 | Vitis编译选项 |

---

## 五、脚本功能矩阵

| 目录 | 综合 | Co-Sim | IP导出 | 板级验证 | 构建 |
|------|:----:|:------:|:------:|:--------:|:----:|
| `hls/` | ✅ | ✅ | ✅ | | |
| `verify/` | | | | ✅ | |
| `build/` | | | | | ✅ |

---

## 六、推荐工作流程

### 6.1 HLS开发流程
```bash
# Step 1: 综合
vivado_hls -f script/hls/run_csynth_bram.tcl

# Step 2: Co-Sim验证
vivado_hls -f script/hls/run_cosim_bram.tcl

# Step 3: IP导出
vivado_hls -f script/hls/run_package_bram.tcl
```

### 6.2 板级验证流程
```tcl
# Vivado Hardware Manager TCL Console:
# Step 1: 连接硬件并创建AXI接口
# (在Vivado GUI中完成)

# Step 2: 运行验证脚本
source script/verify/board_verify_complete.tcl
```

### 6.3 Vitis构建流程
```bash
# 需要安装平台
python script/build/build_xclbin.py
```

---

## 七、文件清单

### hls/ (12文件)
```
csynth_standalone.tcl
run_cosim_bram.tcl
run_cosim_system.tcl
run_csynth_bram.py
run_csynth_bram.tcl
run_csynth_calc_image_integrated.tcl
run_csynth.py
run_csynth_socs.tcl
run_csynth_system.tcl
run_csynth.tcl
run_package_bram.tcl
run_package_system.tcl
```

### verify/ (10文件)
```
board_verify_complete.tcl
board_verify_hardware_manager.tcl
board_verify_quick.tcl
board_verify.tcl
board_verify_vivado.tcl
board_verify_xsct.tcl
board_verify_xsdb.tcl
bram_full_test.tcl
manual_test.tcl
test_commands.tcl
```

### build/ (3文件)
```
build_xclbin.py
build_xclbin.sh
compile_standalone_test.py
```

### config/ (9文件)
```
hls_config_bram.cfg
hls_config_bram_simple.cfg
hls_config_bram_vitis.cfg
hls_config.cfg
hls_config_full.cfg
hls_config_system.cfg
hls_integrated.cfg
hls_socs.cfg
v++_config.ini
```

---

*重组日期: 2026-04-04*