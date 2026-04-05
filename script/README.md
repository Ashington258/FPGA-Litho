# Script 目录分类说明

本文档对 `script/` 目录下的脚本按功能进行分类整理。

---

## 目录结构

```
script/
├── hls/              # HLS综合与仿真脚本 (12个文件)
├── verify/           # 板级验证脚本 (4个文件)
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

## 二、板级验证脚本 (`verify/`) ⭐

### 2.1 BRAM硬件测试脚本

| 文件 | 功能 | 状态 |
|------|------|------|
| `bram_test.tcl` | ✅ **BRAM Litho硬件测试** | **推荐使用** |
| `generate_test_data.py` | 测试数据生成 | 辅助工具 |
| `QUICK_REFERENCE.md` | 快速参考指南 | 文档 |
| `README_BOARD_VERIFY.md` | 验证说明文档 | 文档 |

### 2.2 BRAM硬件测试运行方法

#### 方法1: Vivado TCL模式（推荐）

```bash
# 进入项目目录
cd /root/project/FPGA/vitis/FPGA-Litho

# 运行测试脚本
vivado -mode tcl -source script/verify/bram_test.tcl
```

#### 方法2: Vivado Hardware Manager TCL Console

```tcl
# 在 Vivado Hardware Manager TCL Console 中:
# 1. 打开 Vivado Hardware Manager
# 2. 连接到硬件服务器 (localhost:3121)
# 3. 打开硬件目标并下载bitstream
# 4. 运行测试脚本:

source script/verify/bram_test.tcl
```

#### 方法3: 独立Tcl脚本执行

```bash
# 使用tclsh运行（需要在Vivado环境外）
tclsh script/verify/bram_test.tcl
```

### 2.3 测试脚本功能

`bram_test.tcl` 基于**官方文档**创建，包含以下测试：

| 测试步骤 | 功能 | 验证内容 |
|---------|------|---------|
| 步骤1-7 | 硬件初始化 | JTAG连接、bitstream下载、AXI核心访问 |
| 步骤8 | CONTROL寄存器 | IP状态验证 (ap_idle=1) |
| 步骤9 | 重置操作 (OP_RESET=9) | BRAM存储重置，返回值验证 |
| 步骤10-11 | 数据加载/读取 | 光源数据加载和验证 |
| 步骤12 | 参数配置 | Lx/Ly/Nx/Ny参数写入和读回验证 |

### 2.4 寄存器映射（官方定义）

基于 `xhls_litho_system_bram_hw.h`:

| 寄存器名称 | 地址 | 大小 | 功能 |
|-----------|------|------|------|
| AP_CTRL | 0x00 | 32-bit | 控制状态寄存器 (ap_start/done/idle/ready) |
| GIE | 0x04 | 32-bit | 全局中断使能 |
| IER | 0x08 | 32-bit | IP中断使能 |
| ISR | 0x0c | 32-bit | IP中断状态 |
| AP_RETURN | 0x10-0x14 | 64-bit | 返回值 (复数) |
| OPERATION | 0x1c | 32-bit | 操作码选择 (0-9) |
| IDX | 0x24 | 32-bit | 数组索引 |
| VAL_R | 0x2c-0x30 | 64-bit | 数据值 (复数) |
| MODE | 0x38 | 32-bit | 计算模式 (1=TCC, 2=SOCS) |
| LX | 0x40 | 32-bit | 频域X尺寸 |
| LY | 0x48 | 32-bit | 频域Y尺寸 |
| NX | 0x50 | 32-bit | TCC/SOCS参数 |
| NY | 0x58 | 32-bit | TCC/SOCS参数 |
| SRCSIZE | 0x60 | 32-bit | 光源尺寸 |
| NKERNELS | 0x68 | 32-bit | SOCS核数量 |

### 2.5 操作码定义

基于 `hls_litho_system_bram.h`:

```tcl
OP_LOAD_SOURCE   = 0  # 加载光源数据
OP_LOAD_MASK     = 1  # 加载mask数据
OP_LOAD_TCC      = 2  # 加载TCC矩阵
OP_LOAD_KERNELS  = 3  # 加载SOCS kernels
OP_LOAD_SCALES   = 4  # 加载SOCS scales
OP_COMPUTE_TCC   = 5  # TCC模式计算
OP_COMPUTE_SOCS  = 6  # SOCS模式计算
OP_READ_IMGF     = 7  # 读取imgf结果
OP_READ_IMG_OUT  = 8  # 读取img_out结果
OP_RESET         = 9  # 重置所有BRAM存储
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

### 6.2 板级验证流程 ⭐

```bash
# 方法1: Vivado TCL模式（推荐）
cd /root/project/FPGA/vitis/FPGA-Litho
vivado -mode tcl -source script/verify/bram_test.tcl

# 方法2: Vivado Hardware Manager TCL Console
# 在 Vivado GUI 中：
#   Window -> Hardware Manager -> Tcl Console
source script/verify/bram_test.tcl
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

### verify/ (4文件) ⭐
```
bram_test.tcl              # ✅ 正确的测试脚本
generate_test_data.py      # 数据生成工具
QUICK_REFERENCE.md         # 快速参考
README_BOARD_VERIFY.md     # 验证说明
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

## 八、测试结果参考

详细的测试结果请参考：
- `doc/BRAM_TEST_SUMMARY.md` - 完整测试报告

测试成功标志：
- ✓ CONTROL寄存器: `0x00000004` (ap_idle=1)
- ✓ 重置操作返回值: `0x3f800000` (IEEE754浮点数 1.0)
- ✓ AXI核心: `hw_axi_1` 可访问
- ✓ 参数验证: 4/4 通过

---

*文档更新日期: 2026-04-05*
*测试验证日期: 2026-04-05*