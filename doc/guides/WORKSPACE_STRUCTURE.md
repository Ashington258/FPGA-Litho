# FPGA-Litho FPGA工作空间文件树说明

> 更新日期: 2026-04-03  
> 项目状态: Phase 5 已完成80% (待板级验证)

---

## 一、顶层目录结构

```
FPGA-Litho/
├── src/                    # HLS源代码目录
├── include/                # 头文件目录
├── testbench/              # 测试平台目录
├── script/                 # 脚本和配置文件
├── host/                   # XRT/OpenCL主机程序
├── doc/                    # 项目文档
├── data/                   # 测试数据
├── ip_export/              # 导出的IP目录
├── hls_*/                  # HLS工程目录 (多个)
├── archive/                # 历史归档
└── README.md               # 项目说明
```

---

## 二、核心目录详解

### 2.1 src/ - HLS源代码

| 文件名                          | 功能说明                          | 状态   |
| ------------------------------- | --------------------------------- | ------ |
| `hls_litho_system.cpp`          | **顶层系统集成** - TCC/SOCS双模式 | ✅ 完成 |
| `hls_calc_image_integrated.cpp` | **频域图像计算** - TCC模式核心    | ✅ 完成 |
| `hls_socs.cpp`                  | **SOCS图像计算** - SOCS模式核心   | ✅ 完成 |
| `hls_tcc.cpp`                   | **TCC矩阵计算** - 包含Pupil函数   | ✅ 完成 |
| `hls_fft_r2c.cpp`               | FFT 实数转复数                    | ✅ 完成 |
| `hls_fft_c2r.cpp`               | FFT 复数转实数                    | ✅ 完成 |
| `hls_shift.cpp`                 | 2D循环移位模块                    | ✅ 完成 |
| `hls_source.cpp`                | 光源生成模块 (Annular/Dipole等)   | ✅ 完成 |
| `hls_mask.cpp`                  | 掩模生成模块                      | ✅ 完成 |
| `hls_top.cpp`                   | FFT顶层测试接口                   | ✅ 完成 |
| `hls_fft_simple.cpp`            | 简化FFT测试                       | ✅ 完成 |

### 2.2 include/ - 头文件

| 文件名                        | 说明                                  |
| ----------------------------- | ------------------------------------- |
| `hls_types.h`                 | **基础类型定义** - 复数类型、常量、宏 |
| `hls_litho_system.h`          | 系统配置常量、参数结构体              |
| `hls_calc_image_integrated.h` | calcImage接口定义                     |
| `hls_socs.h`                  | SOCS模块接口定义                      |
| `hls_tcc.h`                   | TCC模块接口定义                       |
| `hls_fft_r2c.h`               | FFT R2C接口                           |
| `hls_fft_c2r.h`               | FFT C2R接口                           |
| `hls_shift.h`                 | 移位模块接口                          |
| `hls_top.h`                   | FFT顶层接口                           |

### 2.3 testbench/ - 测试平台

| 文件名                         | 测试目标         | 测试内容             |
| ------------------------------ | ---------------- | -------------------- |
| `litho_system_tb.cpp`          | **系统集成测试** | TCC+SOCS双模式       |
| `calc_image_integrated_tb.cpp` | 频域图像计算     | 3个测试用例          |
| `socs_tb.cpp`                  | SOCS计算         | Kernel-Mask乘法验证  |
| `tcc_tb_hls.cpp`               | TCC矩阵计算      | 对称性、非零性验证   |
| `fft_tb.cpp`                   | FFT模块          | 正弦波/随机/常数测试 |
| `fft_tb_simple.cpp`            | 简化FFT测试      | 基本功能验证         |

### 2.4 script/ - 脚本和配置

| 文件名                   | 用途                       |
| ------------------------ | -------------------------- |
| `hls_config_system.cfg`  | **系统级HLS配置** (主配置) |
| `hls_config.cfg`         | FFT模块配置                |
| `hls_socs.cfg`           | SOCS模块配置               |
| `hls_integrated.cfg`     | calcImage配置              |
| `run_csynth_system.tcl`  | 系统综合TCL脚本            |
| `run_cosim_system.tcl`   | RTL协同仿真脚本            |
| `run_package_system.tcl` | IP打包脚本                 |
| `vitis-comp.json`        | Vitis编译器配置            |

### 2.5 host/ - 主机程序

| 文件名                  | 说明            | 平台               |
| ----------------------- | --------------- | ------------------ |
| `litho_host.cpp`        | **XRT C++主机** | Xilinx FPGA (推荐) |
| `litho_host_opencl.cpp` | OpenCL C++主机  | 通用兼容           |
| `litho_host.py`         | Python XRT主机  | 快速原型验证       |
| `Makefile`              | 构建脚本        | Linux/Windows      |
| `README.md`             | 使用文档        | -                  |

### 2.6 doc/ - 项目文档

| 文件名                             | 内容              |
| ---------------------------------- | ----------------- |
| `PROJECT_SUMMARY.md`               | **项目总结报告**  |
| `calc_image_csynth_analysis.md`    | calcImage综合分析 |
| `calc_image_integration_report.md` | calcImage集成报告 |

---

## 三、HLS工程目录

### 3.1 主要工程

| 目录                              | 功能                      | 状态       |
| --------------------------------- | ------------------------- | ---------- |
| `hls_litho_system_proj/`          | **系统集成工程** (最终版) | ✅ 综合通过 |
| `hls_calc_image_integrated_proj/` | 频域计算工程              | ✅ 综合通过 |
| `hls_socs_proj/`                  | SOCS计算工程              | ✅ 综合通过 |
| `hls_top_simple/`                 | FFT简化测试工程           | ✅ CSIM通过 |

### 3.2 测试工程

| 目录                                 | 测试内容              |
| ------------------------------------ | --------------------- |
| `hls_system_test/`                   | 系统测试 (CSIM/COSIM) |
| `hls_system_test2/`                  | 系统测试备份          |
| `hls_calc_image_integrated_test/`    | calcImage测试         |
| `hls_socs_test/` ~ `hls_socs_test5/` | SOCS多版本测试        |

### 3.3 工程目录内部结构

```
hls_litho_system_proj/
├── hls.app               # HLS应用配置
├── solution1/             # 解决方案目录
│   ├── impl/              # 实现结果
│   │   ├── ip/            # 导出的IP ⭐
│   │   ├── verilog/       # 生成的Verilog
│   │   └── report/        # 综合报告
│   ├── solution1.data/    # 解决方案数据
│   └── solution1_data.json
├── logs/                  # 日志文件
└── .autopilot/            # AutoPilot缓存
```

---

## 四、关键文件路径

### 4.1 最终输出

```
# Vivado IP包位置
hls_litho_system_proj/solution1/impl/ip/
├── component.xml          # IP定义
├── fpga-litho_org_hls_hls_litho_system_1_0.zip  # IP压缩包
├── hdl/verilog/           # Verilog源码 (95个文件)
├── hdl/vhdl/              # VHDL源码 (92个文件)
├── drivers/               # 驱动文件 (10个)
└── xgui/                  # GUI配置界面
```

### 4.2 综合报告

```
# 系统综合报告
hls_litho_system_proj/solution1/solution1.dir/report/

# 模块综合报告
hls_calc_image_integrated_proj/solution1/.../report/
hls_socs_proj/solution1/.../report/
```

---

## 五、工作流程指引

### 5.1 C仿真验证

```bash
cd E:/1.Project/4.FPGA/vitis/FPGA_Litho/FPGA-Litho
vitis-run --mode hls --csim --config script/hls_config_system.cfg --work_dir hls_top_simple
```

### 5.2 C综合

```bash
vitis-run --mode hls --csynth --config script/hls_config_system.cfg --work_dir hls_litho_system_proj
```

### 5.3 RTL协同仿真

```bash
vitis-run --mode hls --cosim --config script/hls_config_system.cfg --work_dir hls_litho_system_proj
```

### 5.4 IP导出

```bash
vitis-run --mode hls --tcl script/run_package_system.tcl --work_dir hls_litho_system_proj
```

### 5.5 Host程序运行

```bash
cd host
make host
./litho_host --xclbin <xclbin_path> --mode 1 --verbose
```

---

## 六、模块依赖关系

```
┌─────────────────────────────────────────────────────────────┐
│                    hls_litho_system (顶层)                   │
│  ┌─────────────────────┐    ┌─────────────────────────────┐ │
│  │   TCC Mode (mode=1) │    │    SOCS Mode (mode=2)       │ │
│  │                     │    │                             │ │
│  │  source ─────────┐  │    │  kernels ──┐               │ │
│  │                  ↓  │    │             ↓               │ │
│  │  hls_tcc ──→ hls_calc_image  │  hls_socs ──→ img_out   │ │
│  │                  │  │    │             │               │ │
│  │  mask_fft ───────┘  │    │  scales ────┘               │ │
│  │         ↓           │    │         ↓                   │ │
│  │      imgf           │    │      img_out                │ │
│  └─────────────────────┘    └─────────────────────────────┘ │
└─────────────────────────────────────────────────────────────┘

基础模块:
├── hls_fft_r2c / hls_fft_c2r  (FFT变换)
├── hls_shift                   (循环移位)
├── hls_source                  (光源生成)
└── hls_mask                    (掩模生成)
```

---

## 七、资源利用率汇总

### 7.1 系统级资源 (xcku3p-ffvb676-2-e)

| 资源 | 使用量 | 总量    | 利用率  |
| ---- | ------ | ------- | ------- |
| BRAM | 615    | 720     | **85%** |
| DSP  | 87     | 1368    | 6%      |
| FF   | 33,368 | 325,440 | 10%     |
| LUT  | 37,315 | 162,720 | 22%     |

### 7.2 时钟性能

| 模块             | 目标频率 | 实际频率 | 状态   |
| ---------------- | -------- | -------- | ------ |
| hls_litho_system | 200 MHz  | 274 MHz  | ✅ +37% |
| hls_calc_image   | 200 MHz  | 274 MHz  | ✅ +37% |
| hls_socs         | 200 MHz  | 290 MHz  | ✅ +45% |
| hls_tcc          | 200 MHz  | 342 MHz  | ✅ +71% |

---

## 八、后续工作清单

### 待办事项

- [ ] **板级验证** - 需要FPGA硬件
  - 生成bitstream
  - 部署到目标板卡
  - 性能基准测试

- [ ] **性能优化**
  - 数据传输优化
  - 内存带宽优化
  - 多核并行优化

### 可选优化

- [ ] FFT IP集成 (替代简化实现)
- [ ] 移位查找表优化
- [ ] URAM大型矩阵存储

---

## 九、注意事项

1. **BRAM利用率85%** - 接近上限，后续扩展需注意
2. **Windows路径长度限制** - IP名称较长可能有警告
3. **数据格式** - 复数采用float32交替存储
4. **Host程序兼容性** - 需要XRT 2022.1+

---

> 本文档记录工作空间文件结构，供后续开发参考。如有变更请及时更新。