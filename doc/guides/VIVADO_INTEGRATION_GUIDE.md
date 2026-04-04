# K-Litho BRAM Version Vivado Integration Guide

## Phase 6F Alternative: Vivado Flow (No xclbin)

由于v++编译需要目标平台（xcku3p board package），在没有平台的情况下，可以使用Vivado集成流程作为替代方案。

---

## 方案对比

| 方案 | 适用场景 | 优势 | 步骤 |
|------|---------|------|------|
| **v++ xclbin** | Alveo/U.2加速卡 | 快速部署 | 1步: v++ link |
| **Vivado IP** | 自定义FPGA板卡 | 灵活集成 | 多步: Vivado流程 |

---

## Vivado集成步骤

### Step 1: 导入HLS IP

```tcl
# Vivado TCL脚本
create_project litho_bram_vivado ./vivado_proj -part xcku3p-ffvb676-2-e

# 设置IP仓库路径
set_property ip_repo_paths {
    ./hls_litho_system_bram_proj/solution1/impl/ip
} [current_project]
update_ip_catalog

# 添加HLS IP
create_bd_cell -type ip -vlnv xilinx.com:hls:hls_litho_system_bram:1.0 litho_kernel
```

### Step 2: Block Design连接

**接口定义**:
```
HLS Kernel接口:
├── S_AXI_CONTROL (AXI-Lite Slave)
│   ├── operation (RW, offset=0x1C)
│   ├── idx (RW, offset=0x24)
│   ├── val (RW, offset=0x2C)
│   ├── mode (RW, offset=0x38)
│   ├── Lx (RW, offset=0x40)
│   ├── Ly (RW, offset=0x48)
│   ├── Nx (RW, offset=0x50)
│   ├── Ny (RW, offset=0x58)
│   ├── srcSize (RW, offset=0x60)
│   ├── nkernels (RW, offset=0x68)
│   └── ap_return (R, offset=0x10)
│
├── ap_clk (Clock)
├── ap_rst_n (Reset)
└── interrupt (Interrupt)
```

**推荐Block Design**:
```
┌─────────────────────────────────────────────────────────┐
│                    xcku3p FPGA                           │
│                                                         │
│  ┌──────────┐    ┌──────────┐    ┌──────────────────┐  │
│  │ PCIe     │───▶│ AXI Inter│───▶│ S_AXI_CONTROL    │  │
│  │ DMA      │    │ connect  │    │                  │  │
│  └──┬───────┘    └────┬─────┘    │  ┌────────────┐  │  │
│     │                │          │  │ HLS Kernel │  │  │
│     │                │          │  │ (BRAM ver) │  │  │
│     │                │          │  └────┬───────┘  │  │
│     │                │          └───────┴──────────┘  │
│     │                │                                 │
│     │                ▼                                 │
│  ┌──┴────────────────┴──┐                             │
│  │   AXI-Lite Master    │ (可选: 用于BRAM直接访问)     │
│  └──────────────────────┘                             │
│                                                        │
└─────────────────────────────────────────────────────────┘
```

### Step 3: 生成Bitstream

```tcl
# 综合和实现
synth_design -top litho_top
opt_design
place_design
route_design

# 生成bitstream
write_bitstream -force litho_bram.bit
```

---

## PCIe驱动集成

### 选项A: AXI-Lite控制

通过PCIe AXI-Lite接口控制HLS内核：

```c
// C驱动示例
#define CTRL_BASE_ADDR 0x40000

typedef struct {
    uint32_t operation;  // offset 0x00
    uint32_t idx;        // offset 0x08
    uint64_t val;        // offset 0x10
    uint32_t mode;       // offset 0x18
    uint32_t Lx;         // offset 0x20
    uint32_t Ly;         // offset 0x28
    uint32_t Nx;         // offset 0x30
    uint32_t Ny;         // offset 0x38
    uint32_t srcSize;    // offset 0x40
    uint32_t nkernels;   // offset 0x48
} LithoBRAMRegs;

void load_mask_data(volatile LithoBRAMRegs *regs, uint32_t idx, float val_r, float val_i) {
    regs->operation = 1;  // OP_LOAD_MASK
    regs->idx = idx;
    regs->val = ((uint64_t)(*(uint32_t*)&val_i) << 32) | (*(uint32_t*)&val_r);
    // 启动执行
    *((uint32_t*)regs + 0x04) = 1;  // ap_start
}
```

### 选项B: JTAG/UART调试

对于开发测试，可以使用JTAG或UART接口：

```tcl
# XSCT调试脚本
connect
targets -set -filter {name =~ "Cortex*"}
load_bitstream litho_bram.bit

# 设置控制寄存器
mwr 0x40000 0  ;# operation = OP_RESET
mwr 0x40004 1  ;# ap_start
```

---

## 验证流程

### 1. 功能验证

使用JTAG加载测试数据：

```tcl
# Test sequence: Load mask -> Compute -> Read result
mwr 0x40000 1  ;# operation = OP_LOAD_MASK
mwr 0x40008 0  ;# idx = 0
mwr 0x40010 0x00000000_0000003F ;# val = (0, 63)
mwr 0x40004 1  ;# ap_start

# 等待完成
while {[mrd 0x40000] & 0x02 == 0} { }
```

### 2. 性能测量

使用Vivado ILA核测量延迟：

```tcl
# 添加ILA
create_bd_cell -type ip -vlnv xilinx.com:ip:ila:1.0 ila_0
set_property -dict {
    CONFIG.C_NUM_OF_PROBE_SLOTS {1}
    CONFIG.C_PROBE0_WIDTH {64}
} [get_bd_cells ila_0]

# 连接ILA到ap_start和interrupt
connect_bd_net [get_bd_pins litho_kernel/ap_start] [get_bd_pins ila_0/probe0]
```

---

## 资源预算

基于HLS综合结果：

| 资源 | HLS核心 | 系统集成 | 总预算 |
|------|---------|---------|--------|
| BRAM_18K | 131 | +20 | 151 (21%) |
| DSP | 27 | +0 | 27 (2%) |
| FF | 5,083 | +2,000 | 7,083 (2%) |
| LUT | 8,346 | +3,000 | 11,346 (7%) |

**xcku3p总资源**: BRAM=720, DSP=1368, FF=276K, LUT=166K

---

## 下一步

1. **安装目标平台** (如果使用v++ xclbin流程):
   ```bash
   # 从AMD官网下载平台
   # 平台名称: xilinx_ku3p_<board>_x.x.x
   ```

2. **或使用Vivado流程**:
   - 打开Vivado 2025.2
   - 创建项目 `xcku3p-ffvb676-2-e`
   - 导入HLS IP包
   - 设计Block Diagram
   - 生成bitstream

---

## 文件位置

| 文件 | 位置 | 用途 |
|------|------|------|
| HLS IP包 | `hls_litho_system_bram_proj/solution1/impl/ip/` | Vivado导入 |
| XO文件 | `hls_litho_system_bram.xo` | v++编译 |
| IP XCI | `impl/ip/hdl/ip/` | 子核集成 |
| VHDL源码 | `impl/ip/hdl/vhdl/` | RTL源码 |
| Verilog源码 | `impl/ip/hdl/verilog/` | RTL源码 |

---

**创建日期**: 2026-04-04
**状态**: Phase 6F准备完成
**下一步**: 根据实际硬件选择集成方案